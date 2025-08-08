import * as pulumi from "@pulumi/pulumi";
import * as aws from "@pulumi/aws";

const config = new pulumi.Config();
const projectName = "telemetry-mqtt";

// Get AWS region and account info
const currentRegion = aws.getRegion();
const callerIdentity = aws.getCallerIdentity();

// HTTP to HTTPS Proxy Infrastructure

// IAM Role for Lambda execution
const lambdaExecutionRole = new aws.iam.Role(`${projectName}-lambda-execution-role`, {
    assumeRolePolicy: JSON.stringify({
        Version: "2012-10-17",
        Statement: [{
            Action: "sts:AssumeRole",
            Effect: "Allow",
            Principal: {
                Service: "lambda.amazonaws.com",
            },
        }],
    }),
});

// Attach basic Lambda execution policy
new aws.iam.RolePolicyAttachment(`${projectName}-lambda-basic-execution`, {
    role: lambdaExecutionRole.name,
    policyArn: "arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole",
});

// Lambda function for backend processing
const backendLambda = new aws.lambda.Function(`${projectName}-backend-lambda`, {
    runtime: aws.lambda.Runtime.NodeJS18dX,
    code: new pulumi.asset.AssetArchive({
        "index.js": new pulumi.asset.StringAsset(`
exports.handler = async (event) => {
    console.log('Received event:', JSON.stringify(event, null, 2));
    
    // Handle both ALB and API Gateway events
    let method, path, headers, body, queryParams;

    if (method === 'GET' && path === '/') {
        return {
            statusCode: 200,
            body: 'OK',
        };
    }
    
    if (event.requestContext && event.requestContext.elb) {
        // ALB event format
        method = event.httpMethod;
        path = event.path;
        headers = event.headers || {};
        body = event.body;
        queryParams = event.queryStringParameters || {};
    } else {
        // API Gateway event format (backward compatibility)
        method = event.httpMethod;
        path = event.path;
        headers = event.headers || {};
        body = event.body;
        queryParams = event.queryStringParameters || {};
    }
    
    // Your backend logic goes here
    const response = {
        timestamp: new Date().toISOString(),
        method: method,
        path: path,
        message: "Request processed successfully via HTTP ALB -> Lambda",
        receivedHeaders: headers,
        receivedBody: body,
        queryParams: queryParams,
        source: event.requestContext?.elb ? "ALB" : "API Gateway"
    };
    
    return {
        statusCode: 200,
        headers: {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type, Authorization'
        },
        body: JSON.stringify(response, null, 2)
    };
};`),
    }),
    handler: "index.handler",
    role: lambdaExecutionRole.arn,
    timeout: 30,
});

// API Gateway REST API
const api = new aws.apigateway.RestApi(`${projectName}-http-proxy-api`, {
    name: `${projectName}-http-proxy`,
    description: "HTTP to HTTPS proxy API",
    endpointConfiguration: {
        types: "REGIONAL",
    },
});

// API Gateway resource for proxy (catch all paths)
const proxyResource = new aws.apigateway.Resource(`${projectName}-proxy-resource`, {
    restApi: api.id,
    parentId: api.rootResourceId,
    pathPart: "{proxy+}",
});

// API Gateway method for ANY HTTP method on proxy resource
const proxyMethod = new aws.apigateway.Method(`${projectName}-proxy-method`, {
    restApi: api.id,
    resourceId: proxyResource.id,
    httpMethod: "ANY",
    authorization: "NONE",
});

// API Gateway method for ANY HTTP method on root resource
const rootMethod = new aws.apigateway.Method(`${projectName}-root-method`, {
    restApi: api.id,
    resourceId: api.rootResourceId,
    httpMethod: "ANY",
    authorization: "NONE",
});

// Integration between API Gateway and Lambda for proxy resource
const proxyIntegration = new aws.apigateway.Integration(`${projectName}-proxy-integration`, {
    restApi: api.id,
    resourceId: proxyResource.id,
    httpMethod: proxyMethod.httpMethod,
    integrationHttpMethod: "POST",
    type: "AWS_PROXY",
    uri: backendLambda.invokeArn,
});

// Integration between API Gateway and Lambda for root resource
const rootIntegration = new aws.apigateway.Integration(`${projectName}-root-integration`, {
    restApi: api.id,
    resourceId: api.rootResourceId,
    httpMethod: rootMethod.httpMethod,
    integrationHttpMethod: "POST",
    type: "AWS_PROXY",
    uri: backendLambda.invokeArn,
});

// API Gateway deployment
const deployment = new aws.apigateway.Deployment(`${projectName}-api-deployment`, {
    restApi: api.id,
    stageName: "prod",
}, {
    dependsOn: [proxyMethod, rootMethod, proxyIntegration, rootIntegration],
});

// Lambda permission for API Gateway to invoke the function
const lambdaPermission = new aws.lambda.Permission(`${projectName}-lambda-api-permission`, {
    statementId: "AllowExecutionFromAPIGateway",
    action: "lambda:InvokeFunction",
    function: backendLambda.name,
    principal: "apigateway.amazonaws.com",
    sourceArn: pulumi.interpolate`${api.executionArn}/*/*`,
});

// ALB HTTP to HTTPS Proxy Infrastructure

// Get default VPC
const defaultVpc = aws.ec2.getVpc({
    default: true,
});

const defaultVpcSubnets = defaultVpc.then(vpc => aws.ec2.getSubnets({
    filters: [{
        name: "vpc-id",
        values: [vpc.id],
    }],
}));

// Security Group for ALB
const albSecurityGroup = new aws.ec2.SecurityGroup(`${projectName}-alb-sg`, {
    namePrefix: `${projectName}-alb-`,
    description: "Security group for HTTP to HTTPS proxy ALB",
    vpcId: defaultVpc.then(vpc => vpc.id),
    
    ingress: [
        {
            description: "HTTP",
            fromPort: 80,
            toPort: 80,
            protocol: "tcp",
            cidrBlocks: ["0.0.0.0/0"],
        },
        {
            description: "HTTPS", 
            fromPort: 443,
            toPort: 443,
            protocol: "tcp",
            cidrBlocks: ["0.0.0.0/0"],
        }
    ],
    
    egress: [
        {
            fromPort: 0,
            toPort: 0,
            protocol: "-1",
            cidrBlocks: ["0.0.0.0/0"],
        },
    ],
});

// Application Load Balancer
const alb = new aws.lb.LoadBalancer(`${projectName}-alb`, {
    name: `iot-alb`, // Much shorter name
    internal: false,
    loadBalancerType: "application",
    securityGroups: [albSecurityGroup.id],
    subnets: defaultVpcSubnets.then(subnets => subnets.ids),
    
    enableDeletionProtection: false,
});

// Target Group for Lambda
const lambdaTargetGroup = new aws.lb.TargetGroup(`${projectName}-lambda-tg`, {
    name: `${projectName}-lambda-tg`,
    targetType: "lambda",
});

// Lambda permission for ALB to invoke the function
const lambdaAlbPermission = new aws.lambda.Permission(`${projectName}-lambda-alb-permission`, {
    statementId: "AllowExecutionFromALB",
    action: "lambda:InvokeFunction",
    function: backendLambda.name,
    principal: "elasticloadbalancing.amazonaws.com",
    sourceArn: lambdaTargetGroup.arn,
});

// Attach Lambda to Target Group
const lambdaTargetGroupAttachment = new aws.lb.TargetGroupAttachment(`${projectName}-lambda-tg-attachment`, {
    targetGroupArn: lambdaTargetGroup.arn,
    targetId: backendLambda.arn,
}, {
    dependsOn: [lambdaAlbPermission],
});

// HTTP Listener (Port 80)
const httpListener = new aws.lb.Listener(`${projectName}-http-listener`, {
    loadBalancerArn: alb.arn,
    port: 80,
    protocol: "HTTP",
    
    defaultActions: [{
        type: "forward",
        targetGroupArn: lambdaTargetGroup.arn,
    }],
});

// Route 53 Custom Domain Setup

// Get the existing hosted zone for autostrux.com
const hostedZone = aws.route53.getZone({
    name: "autostrux.com",
});

// Create an alias record pointing api.autostrux.com to the ALB
const apiRecord = new aws.route53.Record(`${projectName}-api-record`, {
    zoneId: hostedZone.then(zone => zone.zoneId),
    name: "api.autostrux.com",
    type: "A",
    
    aliases: [{
        name: alb.dnsName,
        zoneId: alb.zoneId,
        evaluateTargetHealth: true,
    }],
});

// Outputs
export const httpProxyUrl = pulumi.all([currentRegion, api.id]).apply(([region, apiId]) => `https://${apiId}.execute-api.${region.name}.amazonaws.com/prod`);
export const httpProxyApiId = api.id;
export const httpProxyApiName = api.name;
export const backendLambdaName = backendLambda.name;
export const backendLambdaArn = backendLambda.arn;

// ALB HTTP Proxy outputs
export const albHttpUrl = pulumi.interpolate`http://${alb.dnsName}`;
export const albDnsName = alb.dnsName;
export const albArn = alb.arn;

// Custom Domain outputs
export const customDomainUrl = "http://api.autostrux.com";
export const customDomainName = "api.autostrux.com";

export const lambdaTargetGroupArn = lambdaTargetGroup.arn;
export const lambdaTargetGroupAttachmentArn = lambdaTargetGroupAttachment.id;

// HTTP to HTTPS Proxy usage documentation
export const proxyUsage = {
    note: "HTTP to Lambda Proxy via ALB - send plain HTTP requests to the custom domain URL, they will be forwarded to Lambda",
    exampleCurl: "curl -X POST http://api.autostrux.com/telemetry -H 'Content-Type: application/json' -d '{\"key\":\"value\"}'",
    customDomain: "http://api.autostrux.com (recommended for ESP32 - short URL)",
    albDirectUrl: "Use albHttpUrl if custom domain doesn't work",
    supportedMethods: ["GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS"],
    paths: "All paths are supported - requests to /any/path will be forwarded to Lambda",
    lambdaProcessing: "Customize the Lambda function code above to implement your backend logic",
    espSetup: "Configure your ESP32 to use http://api.autostrux.com (only 26 characters!)"
};

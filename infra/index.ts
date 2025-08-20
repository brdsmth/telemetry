import * as pulumi from "@pulumi/pulumi";
import * as aws from "@pulumi/aws";

// Infrastructure for telemetry system using API Gateway + EventBridge

const config = new pulumi.Config();
const projectName = "telemetry-mqtt";

// Get AWS region and account info
const currentRegion = aws.getRegion();
const callerIdentity = aws.getCallerIdentity();

// API Gateway - Imported existing API
const api = new aws.apigateway.RestApi("telemetry-mqtt-http-proxy-api", {
    apiKeySource: "HEADER",
    description: "HTTP to HTTPS proxy API",
    endpointConfiguration: {
        ipAddressType: "ipv4",
        types: "REGIONAL",
    },
    name: "telemetry-mqtt-http-proxy",
    putRestApiMode: "overwrite",
}, {
    protect: true,
});

// IAM Role for API Gateway to EventBridge integration - Imported existing role
const apiGatewayEventBridgeRole = new aws.iam.Role("telemetry-mqtt-gateway-to-eventbridge-role", {
    assumeRolePolicy: JSON.stringify({
        Version: "2012-10-17",
        Statement: [{
            Action: "sts:AssumeRole",
            Effect: "Allow",
            Principal: {
                Service: "apigateway.amazonaws.com"
            }
        }]
    }),
    inlinePolicies: [{
        name: "PutEventsToBus",
        policy: JSON.stringify({
            Version: "2012-10-17",
            Statement: [{
                Action: "events:PutEvents",
                Effect: "Allow",
                Resource: "arn:aws:events:us-east-1:724335082787:event-bus/telemetry-bus"
            }]
        }),
    }],
    name: "telemetry-gateway-to-eventbridge-role",
}, {
    protect: true,
});

// API Gateway resource for /ingest - Imported existing resource
const ingestResource = new aws.apigateway.Resource("telemetry-mqtt-ingest-resource", {
    parentId: "2b6qcqdus5",
    pathPart: "ingest",
    restApi: "s5lyg1baa0",
}, {
    protect: true,
});

// API Gateway method for POST /ingest - Imported existing method
const ingestMethod = new aws.apigateway.Method("telemetry-mqtt-ingest-method", {
    authorization: "NONE",
    httpMethod: "POST",
    resourceId: "rj4oky",
    restApi: "s5lyg1baa0",
}, {
    protect: true,
});

// Integration between API Gateway and EventBridge - Imported existing integration
const ingestIntegration = new aws.apigateway.Integration("telemetry-mqtt-ingest-integration", {
    cacheNamespace: "rj4oky",
    connectionType: "INTERNET",
    credentials: "arn:aws:iam::724335082787:role/telemetry-gateway-to-eventbridge-role",
    httpMethod: "POST",
    integrationHttpMethod: "POST",
    passthroughBehavior: "NEVER",
    requestParameters: {
        "integration.request.header.Content-Type": "'application/x-amz-json-1.1'",
        "integration.request.header.X-Amz-Target": "'AWSEvents.PutEvents'",
    },
    requestTemplates: {
        "application/json": `{
            "Entries": [
                {
                "Source": "$input.params('x-source')",
                "DetailType": "$input.params('x-detail-type')",
                "Detail": "$util.escapeJavaScript($input.body)",
                "EventBusName": "telemetry-bus"
                }
            ]
        }`
    },
    resourceId: "rj4oky",
    restApi: "s5lyg1baa0",
    type: "AWS",
    uri: "arn:aws:apigateway:us-east-1:events:action/PutEvents",
}, {
    protect: true,
});

// Method response for POST /ingest - Imported existing method response
const ingestMethodResponse = new aws.apigateway.MethodResponse("telemetry-mqtt-ingest-method-response", {
    httpMethod: "POST",
    resourceId: "rj4oky",
    responseModels: {
        "application/json": "Empty",
    },
    restApi: "s5lyg1baa0",
    statusCode: "200",
}, {
    protect: true,
});

// Integration response for POST /ingest - Imported existing integration response
const ingestIntegrationResponse = new aws.apigateway.IntegrationResponse("telemetry-mqtt-ingest-integration-response", {
    httpMethod: "POST",
    resourceId: "rj4oky",
    responseTemplates: {
        "application/json": "{ \"ok\": true }",
    },
    restApi: "s5lyg1baa0",
    statusCode: "200",
}, {
    protect: true,
});

// API Gateway deployment - Imported existing deployment
const deployment = new aws.apigateway.Deployment("telemetry-mqtt-api-deployment", {
    restApi: "s5lyg1baa0"
}, {
    protect: true,
});

// Outputs
export const apiGatewayUrl = pulumi.all([currentRegion, api.id]).apply(([region, apiId]) => `https://${apiId}.execute-api.${region.name}.amazonaws.com/prod`);
export const apiGatewayId = api.id;
export const apiGatewayName = api.name;
export const ingestEndpoint = pulumi.all([currentRegion, api.id]).apply(([region, apiId]) => `https://${apiId}.execute-api.${region.name}.amazonaws.com/prod/ingest`);
export const iamRoleArn = apiGatewayEventBridgeRole.arn;
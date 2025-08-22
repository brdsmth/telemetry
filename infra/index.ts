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

// EventBridge custom bus
const telemetryEventBus = new aws.cloudwatch.EventBus("telemetry-bus", {
    name: "telemetry-bus",
});

// Dead Letter Queue
const telemetryDlq = new aws.sqs.Queue("telemetry-dlq", {
    name: "telemetry-processing-dlq", 
    messageRetentionSeconds: 1209600, // 14 days
});

// SQS Queue for telemetry messages
const telemetryQueue = new aws.sqs.Queue("telemetry-queue", {
    name: "telemetry-processing-queue",
    visibilityTimeoutSeconds: 60,
    messageRetentionSeconds: 1209600, // 14 days
    receiveWaitTimeSeconds: 20, // Long polling
    redrivePolicy: telemetryDlq.arn.apply(dlqArn => JSON.stringify({
        deadLetterTargetArn: dlqArn,
        maxReceiveCount: 3
    })),
});

// IAM role for EventBridge to send messages to SQS
const eventBridgeToSqsRole = new aws.iam.Role("eventbridge-to-sqs-role", {
    name: "telemetry-eventbridge-to-sqs-role",
    assumeRolePolicy: JSON.stringify({
        Version: "2012-10-17",
        Statement: [{
            Action: "sts:AssumeRole",
            Effect: "Allow",
            Principal: {
                Service: "events.amazonaws.com"
            }
        }]
    }),
    inlinePolicies: [{
        name: "SendToSqs",
        policy: telemetryQueue.arn.apply(queueArn => JSON.stringify({
            Version: "2012-10-17",
            Statement: [{
                Action: [
                    "sqs:SendMessage",
                    "sqs:GetQueueAttributes"
                ],
                Effect: "Allow",
                Resource: queueArn
            }]
        }))
    }]
});

// EventBridge rule to send all telemetry events to SQS
const telemetryEventRule = new aws.cloudwatch.EventRule("telemetry-event-rule", {
    name: "telemetry-to-queue-rule",
    description: "Route all telemetry events to SQS queue for processing",
    eventBusName: telemetryEventBus.name,
    eventPattern: JSON.stringify({
        source: ["telemetry", "gateway", "sensor"] // Match common source patterns
    }),
    state: "ENABLED"
});

// EventBridge target - SQS queue
const telemetryEventTarget = new aws.cloudwatch.EventTarget("telemetry-event-target", {
    rule: telemetryEventRule.name,
    eventBusName: telemetryEventBus.name,
    arn: telemetryQueue.arn,
    roleArn: eventBridgeToSqsRole.arn
});

// IAM role for Lambda execution
const lambdaExecutionRole = new aws.iam.Role("telemetry-lambda-execution-role", {
    name: "telemetry-lambda-execution-role",
    assumeRolePolicy: JSON.stringify({
        Version: "2012-10-17", 
        Statement: [{
            Action: "sts:AssumeRole",
            Effect: "Allow",
            Principal: {
                Service: "lambda.amazonaws.com"
            }
        }]
    }),
    managedPolicyArns: [
        "arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole"
    ],
    inlinePolicies: [{
        name: "SqsAccess",
        policy: telemetryQueue.arn.apply(queueArn => JSON.stringify({
            Version: "2012-10-17",
            Statement: [{
                Action: [
                    "sqs:ReceiveMessage",
                    "sqs:DeleteMessage", 
                    "sqs:GetQueueAttributes"
                ],
                Effect: "Allow",
                Resource: queueArn
            }]
        }))
    }]
});

// Lambda function to process telemetry messages
const telemetryProcessorLambda = new aws.lambda.Function("telemetry-processor", {
    name: "telemetry-processor",
    runtime: "nodejs18.x",
    role: lambdaExecutionRole.arn,
    handler: "index.handler",
    code: new pulumi.asset.AssetArchive({
        "index.js": new pulumi.asset.StringAsset(`
exports.handler = async (event) => {
    console.log('Processing telemetry batch:', JSON.stringify(event, null, 2));
    
    for (const record of event.Records) {
        try {
            // Parse the EventBridge event from SQS message
            const eventBridgeEvent = JSON.parse(record.body);
            const telemetryData = JSON.parse(eventBridgeEvent.detail);
            
            console.log('Processing telemetry data:', {
                source: eventBridgeEvent.source,
                detailType: eventBridgeEvent['detail-type'],
                data: telemetryData
            });
            
            // TODO: Add your telemetry processing logic here
            // Examples:
            // - Store in database
            // - Transform data
            // - Send to analytics service
            // - Trigger alerts based on thresholds
            
        } catch (error) {
            console.error('Error processing record:', error);
            console.error('Record body:', record.body);
            // Depending on your error handling strategy, you might want to:
            // - Throw error to send message to DLQ
            // - Log and continue processing other messages
            throw error; // This will send the message to DLQ after max retries
        }
    }
    
    return {
        batchItemFailures: [] // Return failed message IDs if partial batch processing
    };
};
        `)
    }),
    timeout: 30,
    description: "Processes telemetry messages from EventBridge via SQS"
});

// Event source mapping - SQS to Lambda
const telemetryEventSourceMapping = new aws.lambda.EventSourceMapping("telemetry-event-source-mapping", {
    eventSourceArn: telemetryQueue.arn,
    functionName: telemetryProcessorLambda.name,
    batchSize: 10,
    maximumBatchingWindowInSeconds: 5,
    functionResponseTypes: ["ReportBatchItemFailures"]
});

// Outputs
export const apiGatewayUrl = pulumi.all([currentRegion, api.id]).apply(([region, apiId]) => `https://${apiId}.execute-api.${region.name}.amazonaws.com/prod`);
export const apiGatewayId = api.id;
export const apiGatewayName = api.name;
export const ingestEndpoint = pulumi.all([currentRegion, api.id]).apply(([region, apiId]) => `https://${apiId}.execute-api.${region.name}.amazonaws.com/prod/ingest`);
export const iamRoleArn = apiGatewayEventBridgeRole.arn;
export const eventBusName = telemetryEventBus.name;
export const queueUrl = telemetryQueue.url;
export const queueArn = telemetryQueue.arn;
export const lambdaFunctionName = telemetryProcessorLambda.name;
export const lambdaFunctionArn = telemetryProcessorLambda.arn;
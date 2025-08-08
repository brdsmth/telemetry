# Telemetry Infrastructure - MQTT

This directory contains Pulumi infrastructure code for deploying an AWS IoT Core MQTT broker to receive telemetry data from your ESP32 gateway.

## Why MQTT?

MQTT is much better suited for IoT devices than HTTP:
- **Smaller packet sizes** (no URL length limits)
- **More efficient** (less overhead)
- **Designed for IoT** (publish/subscribe model)
- **Better reliability** (QoS levels)
- **Lower power consumption**

## Prerequisites

1. **Pulumi CLI**: Install from https://www.pulumi.com/docs/install/
2. **Node.js**: Version 16 or higher
3. **AWS CLI**: Configured with appropriate credentials
4. **AWS Account**: With permissions to create IoT Core, IAM, and CloudWatch resources

## Setup

1. **Install dependencies**:
   ```bash
   npm install
   ```

2. **Configure AWS region** (optional, defaults to your AWS CLI region):
   ```bash
   pulumi config set aws:region us-east-1
   ```

3. **Login to Pulumi** (if using Pulumi Cloud):
   ```bash
   pulumi login
   ```

## Deployment

1. **Preview changes**:
   ```bash
   pulumi preview
   ```

2. **Deploy**:
   ```bash
   pulumi up
   ```

3. **Get the MQTT endpoint**:
   ```bash
   pulumi stack output iotEndpointUrl
   ```

## What gets created

- **AWS IoT Core**: MQTT broker endpoint
- **IoT Thing**: Represents your ESP32 gateway
- **IoT Certificate**: For secure authentication
- **IoT Policy**: Permissions for the gateway
- **Topic Rules**: Process and log telemetry data
- **CloudWatch Log Group**: For telemetry logs

## MQTT Connection Details

After deployment, you'll get:
- **MQTT Endpoint**: Your IoT Core broker URL
- **Thing Name**: `gateway-001`
- **Topic**: `telemetry/gateway-001/data`
- **Port**: 8883 (TLS) or 1883 (non-TLS)

## Testing

You can test the MQTT connection using the AWS CLI:

```bash
# Get the endpoint
ENDPOINT=$(pulumi stack output iotEndpointUrl)

# Test connection (requires AWS IoT certificate)
aws iot-data publish \
  --topic "telemetry/gateway-001/data" \
  --payload '{"node":"test","depth":1,"firmware":"1.0.0","timestamp":1234567890,"value":42.5,"type":"temperature"}' \
  --endpoint-url $ENDPOINT
```

## Monitoring

View telemetry logs:
```bash
aws logs tail /aws/iot/telemetry --follow
```

## Gateway Integration

Update your ESP32 gateway to use MQTT instead of HTTP. The gateway will:

1. Connect to the MQTT broker
2. Publish telemetry data to `telemetry/gateway-001/data`
3. Use the same JSON payload format
4. Benefit from smaller packet sizes and better reliability

## Cleanup

To destroy all resources:
```bash
pulumi destroy
```

## Next Steps

1. **Deploy the infrastructure**
2. **Generate certificates** for your ESP32
3. **Update gateway code** to use MQTT
4. **Test the connection**
5. **Monitor telemetry data** 
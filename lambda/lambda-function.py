import json
import boto3
from boto3.dynamodb.conditions import Key
from datetime import datetime

iot_client = boto3.client('iot-data')
dynamodb = boto3.resource('dynamodb')
timestream_write = boto3.client('timestream-write')

def get_user(user, table):
    response = table.scan(
        ExpressionAttributeValues={
        ':u': user,
        },
        FilterExpression = 'user_id = :u'
        )
        
    return response.get('Items',[])
        
def publish_topic(payload):
    iot_client.publish(
        topic="esp32/open_door",
        qos=1,
        payload=payload
    )
    
def get_user_name(users):
    if users:
        return users[0].get("name")
        
    return "Usuario invalido"
        
def is_valid_user(users):
    
    if users:
        return "1"
        
    return "0"
    
def post_log_database(rfid):
    data = datetime.now()
    string_data = data.strftime("%Y-%m-%d %H:%M:%S")
    table_log = dynamodb.Table('GP_access_db')
    response_db = table_log.put_item(Item={'sensor_a0':string_data, 'rfid':rfid}) 

def post_log_database_timestream(rfid, valid):
    dimensions = [
        {'Name': 'rfid', 'Value': str(rfid)}
    ]
    measure_name = 'is_valid'

    # Write data to Timestream
    timestream_write.write_records(
        DatabaseName='dgp_access_DB',
        TableName='DGP_access_log',
        Records=[
            {
                'Dimensions': dimensions,
                'MeasureName': measure_name,
                'MeasureValue': valid,
                'MeasureValueType': 'BIGINT',
                'Time': str(int(datetime.now().timestamp() * 1000))
            }
        ]
    )
    
def lambda_handler(event, context):
    rfid = "{}".format(event["sensor_a0"]).strip()
    
    user_array = get_user(rfid, dynamodb.Table('GP_access_granted_users'))
    
    valid = is_valid_user(user_array);
    
    payload = json.dumps({'is_valid': valid, 'rfid': rfid, 'name': get_user_name(user_array)}, indent=4)
    
    print(payload)
    
    publish_topic(payload)
    
    post_log_database_timestream(rfid, valid)
        
    return "Done."

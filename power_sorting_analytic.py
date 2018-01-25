#!/usr/bin/python

import paho.mqtt.client as mqtt
import string
##import thingspeak
import time
import httplib, urllib

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded in
# manual interface.
totalpower=0 #total power consumption
p3=0 #user total power consumption
p2=0 #current total power consumption after data push in
p1=0 #previous total power consumption
	 #p2-p1 == rate of change of power consumption
counter=0 #countdown time to POST data to cloud

threshold=100000 #default total power threshold to send alert msg
now=time.time()	#initiate timer
Mrec = 0.0001	#mean recorded power(initiate as float)
meanPower=0		#current mean power
ratio= [0]*1440	#ratio array for whole day
# ratio for daily live update
Rrec = [0]*1440	#ratio array recorded for whole day
# recorded predict
recordCount = 0
reset = 0
#reset default put 1440 to stop record
run=True

# The callback for when the client receives a CONNACK respon
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    # Subscribing in on_connect() means that if we lose th
    # reconnect then subscriptions will be renewed.
    client.subscribe("digitalHome/devices/#")
    client.subscribe("digitalHome/Alert")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    topicDevice='devices'
    if(msg.topic.find(topicDevice)>3):
        msg.topic = msg.topic.translate(None, 'digitalHome/devices/')
        data = str(msg.payload).split("_") #parse income data
        field1 = int(data[0])	#pstatus
        field2 = float(data[1])	#preal
        field3 = int(data[2])	#pmode
        field8 = str(data[3])	#user list
		#total up real power
        def addPower():
            x=field2
            def totalAdd():
                global totalpower
                totalpower+=x
            totalAdd()
        addPower()
        userlist='+1'
        if(field8.find(userlist)>1): #check if user found
        def addPower2():
            x=field2
            def userPower():
                global p3
                p3+=x
            userPower()
        addPower2()
    else:
        def chgThres():
            x=long(msg.payload)
            def replace():
                global threshold
                threshold = x
            replace()
        chgThres()
    print msg.topic

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("broker.hivemq.com", 1883, 60)


while run:
    if(time.time()-now)>60:
        ###plot power consumption graph
        def powerSent(x):
            graph=str(x/60)
            params = "/update?api_key=BVA7P1HO2G97Q6TG"+"&field3="+graph
            headers = {"Content-typ": "application/x-www-form-urlencoded", "Accept": "text/plain"}
            conn = httplib.HTTPConnection("iotfoe.ddns.net:3333")
            conn.request("POST", params, "", headers)
            response = conn.getresponse()
            data = response.read()
            conn.close()
        powerSent(p3)

        p2+=totalpower
        if((totalpower/1000)>threshold):
            print "ggspam"
            params = "/trigger/detected/with/key/bmnSW2DacKuV9ej6oI6TX3"
            headers = {"Content-typ": "application/x-www-form-urlencoded", "Accept": "text/plain"}
            conn = httplib.HTTPConnection("maker.ifttt.com:80")
            conn.request("GET", params, "", headers)
            response = conn.getresponse()
            data = response.read()

		client.loop()

        now=time.time()
        if( recordCount< 1440): #count for current
            if(counter<4):
                #1 counter = 5min (default:9[10min])
                a=0
            else:
                ratio[recordCount]=(p2-p1)/300
                #dividen 12 due to 12x5min=1hour counter(default:6[10x6=1h])
                def sentIot(x):
                    graph=str(Rrec[x])
                    params = "/update?api_key=BVA7P1HO2G97Q6TG"+"&field1="+graph
                    headers = {"Content-typ": "application/x-www-form-urlencoded", "Accept": "text/plain"}
                    conn = httplib.HTTPConnection("iotfoe.ddns.net:3333")
                    conn.request("POST", params, "", headers)
                    response = conn.getresponse()
                    data = response.read()
                    conn.close()
                sentIot(recordCount)
                p1=p2
                counter=0
                #alertUser(ratio)
                #ratio will be used to compare threshold ratio
                #alertUser will publish warnning trigger

            client.loop()

            counter+=1
            if( reset < 1440): #recount for new day
                meanPower += totalpower
                Mrec += (meanPower/1440)
                Rrec[recordCount]=(p2-p1)/60
                def sentIot2(x):
                    graph=str(Rrec[x])
                    params = "/update?api_key=BVA7P1HO2G97Q6TG"+"&field2="+graph
                    headers = {"Content-typ": "application/x-www-form-urlencoded", "Accept": "text/plain"}
                    conn = httplib.HTTPConnection("iotfoe.ddns.net:3333")
                    conn.request("POST", params, "", headers)
                    response = conn.getresponse()
                    data = response.read()
                    conn.close()
                sentIot2(recordCount)
                reset += 1
            recordCount += 1
        #1min publish once to thingspeak
    client.loop()

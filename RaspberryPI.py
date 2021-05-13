
from rpi_ws281x import *
import threading, discord, colorsys, sys, os, time, socket, websockets, uuid, asyncio, math, socket
import sounddevice as sd
import numpy as np
from math import fmod, cos
PI = math.pi
sign = lambda x: (1, -1)[x<0]

SEG_COUNT = 4
SPEED = 1
STRIP_COLOR = (0, 0, 0)
MODE = "off"
LED_PIN = 12
LED_COUNT = 120

def RGBtoPWM(r, g, b):
    return (min(255,int(g))<<16) + (min(int(r),255)<<8) + int(min(b,255))

def colorFromVal(timer, seg, x):
    t = (1-cos(PI*abs(fmod(seg*x/LED_COUNT+1+timer,1))))/2.0
    return RGBtoPWM(*[int(i * 255) for i in colorsys.hsv_to_rgb(t, 1, 1)])

def setAll(strip, r, g, b):
    for i in range(LED_COUNT):
        strip.setPixelColor(i, RGBtoPWM(r, g, b))

def controlLights():
    global MODE, STRIP_COLOR, SPEED, SEG_COUNT
    strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, 800000, 10, False, 255, 0)
    strip.begin()
    while True:
        if MODE == "off":
            setAll(strip, 0, 0, 0)
        elif MODE == "white":
            setAll(strip, 255, 255, 255)
        elif MODE == "RGB":
            setAll(strip, STRIP_COLOR[0], STRIP_COLOR[1], STRIP_COLOR[2])
        elif MODE == "rainbow":
            for x in range(LED_COUNT):
                strip.setPixelColor(x, colorFromVal(fmod(time.time()*SPEED/SEG_COUNT,1), SEG_COUNT, x))
        elif MODE == "staticRGB":
            for x in range(LED_COUNT):
                strip.setPixelColor(x, colorFromVal(fmod(time.time()*abs(SPEED)/3,1), 0, 1))
                #strip.setPixelColor(x, colorFromVal(STRIP_COLOR[0]/255.0,0,0))
        strip.show()
        time.sleep(1 / 100)
threading.Thread(target = controlLights).start()

async def sendAll(s):
    global MODE, STRIP_COLOR, SPEED, SEG_COUNT
    s = s.replace(' ', '').lower().replace('staticrgb', 'staticRGB').replace('rgb', 'RGB').replace('setspeed', 'setSpeed')
    DNIDH = {i[1]['name'].lower(): i[1]["websocket"] for i in clients.items() if "name" in i[1]}
    keyList = list(DNIDH.keys())
    for i in s.split(';'):
        j = [t.split(',') for t in i.split('>')]
        if len(j) == 1: j = [keyList + ["room"], j[0]] #Set to all devices if none are given
        for a in j[1]:
            if a.startswith("#"): a = "RGB|"+'|'.join(str(int(a[h+1:h+3], 16)) for h in range(0, 6, 2)) #Hex to RGB
            for d in j[0]:
                if d in keyList:
                    await DNIDH[d].send(a)
                elif d == "room": #process room LEDs
                    if a in ["white", "rainbow", "staticRGB", "off"]:
                        MODE = a
                    elif a.startswith("RGB"):
                        MODE = "RGB"
                        STRIP_COLOR = [int(c) for c in a.split('|')[1:]]
                    elif a.startswith("setSpeed"):
                        SPEED = float(a.split('|')[1])

clients = {}
async def server(w, path):
    if path: print(path)

    if path.startswith('/WS'):
        while True:
            data = await w.recv()
            print(data)
            await sendAll(data)
    elif path.startswith('/'):
        await sendAll(path.lstrip('/'))
    else:
        ID = uuid.uuid4().hex
        print(ID, "- Connected")
        clients[ID] = {"websocket": w, "name": None}
        await w.send("getDevice")
        try:
            while True:
                data = await w.recv()
                if data == "getTime":
                    print(ID, "- Got time request.")
                    await w.send("TIME|"+str(time.time()))
                elif data.startswith("device"):
                    clients[ID]["name"] = data.split('|')[1]
                    print(ID, f'- Identified device "{clients[ID]["name"]}"')
                    if clients[ID]["name"] == "Lamp":
                        await w.send("setSegments|1")
                        #await w.send("setSpeed|-0.45")
        except Exception as e:
            print(ID, "- Disconnected", f"({clients[ID]['name']})", e)
            del clients[ID]

async def timer():
    while True:
        for i in clients:
            await clients[i]['websocket'].send("resetTime")
        await asyncio.sleep(5 * 60)

bot = discord.Client()
@bot.event
async def on_ready():
    print("Bot ready.")
    await sendAll("off")

@bot.event
async def on_message(msg):
    if msg.channel.id != REPLACEWITHCONTROLCHANNELID:
        return
    await sendAll(msg.content)

async def main():
    runner = websockets.serve(server,"192.168.1.25",213,ping_interval=1,ping_timeout=5,close_timeout=4)
    asyncio.gather(runner, timer())
    await bot.start("TOKEN")
asyncio.get_event_loop().run_until_complete(main())

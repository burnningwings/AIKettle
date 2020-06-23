import time

from django.http import HttpResponse
from django.shortcuts import render
import serial
import re
import json


def index(request):
    if request.method == 'POST':
        return HttpResponse('error')

    if request.method == 'GET':
        return render(request, 'index.html')

def add_water(request):
    if request.method == 'POST':
        pass

    if request.method == 'GET':
        status = request.GET.get("add_water_status")
        try:
            ser = serial.Serial("COM2", 9600)
            if status == 'true':
                ser.write("4".encode("gbk"))
                ser.close()
                return HttpResponse("on")
            else:
                ser.write("5".encode("gbk"))
                ser.close()
                return HttpResponse("off")

        except serial.SerialException:
            return HttpResponse('loss')


def reduce_water(request):
    if request.method == 'POST':
        pass

    if request.method == 'GET':
        try:
            ser = serial.Serial("COM2", 9600)
            ser.write("6".encode("gbk"))
            ser.close()
            return HttpResponse('success')
        except serial.SerialException:
            return HttpResponse("loss")


def power(request):
    if request.method == 'POST':
        pass

    if request.method == 'GET':
        power_status = request.GET.get("power_status")
        print(power_status)
        try:
            ser = serial.Serial("COM2", 9600, timeout=3)
            if power_status == "true":
                ser.write("1".encode("gbk"))
                ser.write("9".encode("gbk"))
                result = ser.readline().decode("gbk")
                print(result)
                ser.close()
                if result == '':
                    return HttpResponse('loss')
                print("开机")
                return HttpResponse("on")
            else:
                ser.write("0".encode("gbk"))
                ser.close()
                print("关机")
                return HttpResponse('off')
        except serial.SerialException:
            return HttpResponse("loss")


def heat(request):
    if request.method == 'POST':
        pass

    if request.method == 'GET':
        heat_status = request.GET.get("heat_status")
        try:
            ser = serial.Serial("COM2", 9600)
            print(heat_status)
            if heat_status == "true":
                ser.write("2".encode("gbk"))
                print("开始烧水")
                return HttpResponse('on')
            else:
                ser.write("3".encode("gbk"))
                print("停止烧水")
                return HttpResponse("off")
        except serial.SerialException:
            return HttpResponse("loss")


def get_info(request):
    if request.method == 'POST':
        pass

    if request.method == 'GET':
        try:
            ser = serial.Serial("COM2", 9600, timeout=1)
            ser.write("9".encode("gbk"))
            s = ser.readline().decode('gbk')
            ser.close()
            if s != '':
                print(s)
                t = re.search(r't:\[(.*?)\]', str(s))
                w = re.search(r'water:\[(.*?)\]', str(s))
                power_status = re.search(r'power:\[(.*?)\]', str(s))

                water_LED = re.search(r'waterLED:\[(.*?)\]', str(s))
                heat_LED = re.search(r'heatLED:\[(.*?)\]', str(s))
                end_LED = re.search(r'endLED:\[(.*?)\]', str(s))
                if water_LED and water_LED.group(1) == '1':
                    work = 'water'
                elif heat_LED and heat_LED.group(1) == '1':
                    work = 'heat'
                elif end_LED and end_LED.group(1) == '1':
                    work = 'end'
                else:
                    work = 'off'

                # 温度:[23],水位:[32]
                if t:
                    t = t.group(1)
                if w:
                    w = w.group(1)
                if power_status:
                    power_status = power_status.group(1)
                return HttpResponse(json.dumps({
                    "status": 'success',
                    "t": t,
                    "w": w,
                    "power": power_status,
                    "work": work
                }))
            return HttpResponse(json.dumps({
                "status": "nothing"
            }))
        except serial.SerialException:
            print("读取端口错误")
        return HttpResponse(json.dumps({
            "status": 'error'
        }))



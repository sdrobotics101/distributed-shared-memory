import sys
sys.path.insert(0, '../build')

import pydsm
import time

client = pydsm.Client(6, 0)
client.registerLocalBuffer("remote0", 4, False)
time.sleep(0.1)
client.setLocalBufferContents("remote0", "start")
data = input("")
while(data != "kill"):
    client.setLocalBufferContents("remote0", data)
    data = input("")

import sys
sys.path.insert(0, '../build')

import pydsm
import time

client = pydsm.Client(2, 0)
client.registerRemoteBuffer("remote0", "127.0.0.1", 6)
time.sleep(5)

data = client.getRemoteBufferContents("remote0", "127.0.0.1", 6)
print("DATA: " + data)

import sys
sys.path.insert(0, '../build')

import pydsm
import argparse
import ipaddress

def validateIP(addr):
    try:
        ipaddress.ip_address(addr)
    except ValueError:
        return False
    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('serverID', type=int)
    parser.add_argument('clientID', type=int)
    args = parser.parse_args()
    client = pydsm.Client(args.serverID, args.clientID, True)
    try:
        while (1):
            tokens = input("> ").split()
            if (tokens[0] == "kill" or tokens[0] == "exit"):
                print("exiting")
                break
            elif (tokens[0] == "regl" and len(tokens) == 3):
                if client.registerLocalBuffer(tokens[1], int(tokens[2]), False):
                    print("Registered new local buffer")
                else:
                    print("Invalid operands")
            elif (tokens[0] == "reglo" and len(tokens) == 3):
                if client.registerLocalBuffer(tokens[1], int(tokens[2]), True):
                    print("Registered new local only buffer")
                else:
                    print("Invalid operands")
            elif (tokens[0] == "regr" and len(tokens) == 4):
                if not validateIP(tokens[2]):
                    print("invalid IP address")
                    continue
                if client.registerRemoteBuffer(tokens[1], tokens[2], int(tokens[3])):
                    print("Registered new remote buffer")
                else:
                    print("Invalid operands")
            elif (tokens[0] == "dcl" and len(tokens) == 2):
                if client.disconnectFromLocalBuffer(tokens[1]):
                    print("Disconnected from local buffer")
                else:
                    print("Invalid operands")
            elif (tokens[0] == "dcr" and len(tokens) == 4):
                if not validateIP(tokens[2]):
                    print("invalid IP address")
                    continue
                if client.disconnectFromRemoteBuffer(tokens[1], tokens[2], int(tokens[3])):
                    print("Disconnected from remote buffer")
                else:
                    print("Invalid operands")
            elif (tokens[0] == "checkl" and len(tokens) == 2):
                size = client.doesLocalExist(tokens[1])
                if size:
                    print("Local buffer exists with size " + str(size))
                else:
                    print("Local buffer does not exist")
            elif (tokens[0] == "checkr" and len(tokens) == 4):
                if not validateIP(tokens[2]):
                    print("invalid IP address")
                    continue
                size = client.doesRemoteExist(tokens[1], tokens[2], int(tokens[3]))
                active = client.isRemoteActive(tokens[1], tokens[2], int(tokens[3]))
                if size:
                    if active:
                        print("Remote buffer is active and has size " + str(size))
                    else:
                        print("Remote buffer is inactive and has size " + str(size))
                else:
                    print("Remote buffer does not exist")
            elif (tokens[0] == "getl" and len(tokens) == 2):
                print(client.getLocalBufferContents(tokens[1]))
            elif (tokens[0] == "setl" and len(tokens) == 3):
                if client.setLocalBufferContents(tokens[1], tokens[2]):
                    print("Set local buffer contents")
                else:
                    print("Invalid operands")
            elif (tokens[0] == "getr" and len(tokens) == 4):
                if not validateIP(tokens[2]):
                    print("invalid IP address")
                    continue
                data, active = client.getRemoteBufferContents(tokens[1], tokens[2], int(tokens[3]))
                if active:
                    print("Active: " + data)
                else:
                    print("Inactive: " + data)
            else:
                print("unknown")
    except KeyboardInterrupt:
        print("")
        print("exiting")

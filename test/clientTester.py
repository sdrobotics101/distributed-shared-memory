import sys
sys.path.insert(0, '../build')

import pydsm
import argparse

# client.registerRemoteBuffer("remote0", "127.0.0.1", 6)

# data = client.getRemoteBufferContents("remote0", "127.0.0.1", 6)
# print("DATA: " + data)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('serverID', type=int)
    parser.add_argument('clientID', type=int)
    args = parser.parse_args()
    client = pydsm.Client(args.serverID, args.clientID)
    while (1):
        tokens = input("> ").split()
        if (tokens[0] == "kill"):
            break
        else if (tokens[0] == "regl" and len(tokens) == 4):
        else if (tokens[0] == "regr" and len(tokens) == 4):
        else if (tokens[0] == "dcl" and len(tokens) == 2):
        else if (tokens[0] == "dcr" and len(tokens) == 4):
        else if (tokens[0] == "checkl" and len(tokens) == 2):
        else if (tokens[0] == "checkr" and len(tokens) == 4):
        else if (tokens[0] == "showl" and len(tokens) == 2):
        else if (tokens[0] == "setl" and len(tokens) == 2):
        else if (tokens[0] == "showr" and len(tokens) == 4):
        else:
            print("unknown")

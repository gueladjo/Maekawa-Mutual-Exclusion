import numpy as np

def compareExecutionTimes(start1, end1, start2, end2):
    print("Comparing %d %d | %d %d" % (start1, end1, start2, end2))
    diff1 = end1 - start1
    if(start2 < start1 + diff1 and start2 > start1):
        return False
    else:
        return True

f = open("config_small.txt", 'r')
if (f.closed):
    print("File not opened")
    exti(1)
s = f.readline()
s = s.split(" ")
numNodes = int(s[0])
numRequests = int(s[3])
f.close()

executionTimes = np.zeros((numNodes, numRequests, 2))

for i in range(0, numNodes):
    f = open("node%doutput.txt" % (i))
    for j in (range(0, numRequests)):
        s = f.readline().split(" ")
        s = map(str.strip, s)
        s = map(int, s)
        executionTimes[i][j][0] = s[0]
        executionTimes[i][j][1] = s[1]
    f.close

for i in range(0, numNodes):
    for j in range(0, numRequests):
        for k in range(i + 1, numNodes):
            for l in range(0, numRequests):
                if (compareExecutionTimes(executionTimes[i][j][0], executionTimes[i][j][1], executionTimes[k][l][0], executionTimes[k][l][1]) == False):
                    print("Failed")
                    exit()

print("Success")
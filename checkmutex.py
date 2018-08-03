import numpy as np
import sys

def compareExecutionTimes(start1_s, start1_ns, end1_s, end1_ns, start2_s, start2_ns, end2_s, end2_ns):
    print("Comparing %d %d %d %d | %d %d %d %d" % (start1_s, start1_ns, end1_s, end1_ns, start2_s, start2_ns, end2_s, end2_ns))
    
    if (start1_s == start2_s):
        diff1 = start1_ns - end1_ns
        if (start2_ns < start1_ns + diff1 and start2_ns > start1_ns):
            return False
        else:
            return True
    else:
        diff1 = start1_s - end1_s
        if (start2_s < start1_s + diff1 and start2_s > start1_s):
            return False
        else:
            return True
    if(start2 < start1 + diff1 and start2 > start1):
        return False
    else:
        return True

def findMinMax(executionTimes, numRequests, numNodes):
    minStart = executionTimes[0][0][0] + (executionTimes[0][0][1] / 1000000000)
    maxEnd = 0 
    for i in range(0, numNodes):
        testMin = executionTimes[i][0][0] + (executionTimes[i][0][1] / 100000000)
        testMax = executionTimes[i][numRequests-1][2] + (executionTimes[i][numRequests-1][3] / 1000000000)
        if (testMin < minStart):
            minStart = testMin
        if (testMax > maxEnd):
            maxEnd = testMax
    return minStart, maxEnd

def sumExecutionTimes(executionTimes, numRequests, numNodes):
    accumulator = 0
    for n in range(0, numNodes):
        for i in range(0, numRequests):
            start = executionTimes[n][i][0] + (executionTimes[n][i][1]/1000000000)
            end = executionTimes[n][i][2] + (executionTimes[n][i][3]/1000000000)
            accumulator += end - start
    return accumulator

print(sys.argv[1])
f = open(sys.argv[1], 'r')
if (f.closed):
    print("File not opened")
    exit(1)

s = f.readline()
print(s)
s = s.split(" ")
numNodes = int(s[0])
numRequests = int(s[3])
f.close()

executionTimes = np.zeros((numNodes, numRequests, 4))
responseTime = np.zeros(numNodes)
messageComplexity = np.zeros(numNodes)

for i in range(0, numNodes):
    f = open("node%doutput" % (i))
    s = f.readline().split(" ")
    responseTime[i] = float(s[0])
    messageComplexity[i] = float(s[1])
    for j in (range(0, numRequests)):
        s = f.readline().split(" ")
        s = map(str.strip, s)
        s = map(int, s)
        executionTimes[i][j][0] = s[0]
        executionTimes[i][j][1] = s[1]
        executionTimes[i][j][2] = s[2]
        executionTimes[i][j][3] = s[3]
    f.close

for i in range(0, numNodes):
    for j in range(0, numRequests):
        for k in range(i + 1, numNodes):
            for l in range(0, numRequests):
                if (compareExecutionTimes(executionTimes[i][j][0], executionTimes[i][j][1],executionTimes[i][j][2], executionTimes[i][j][3], executionTimes[k][l][0], executionTimes[k][l][1], executionTimes[k][l][2], executionTimes[k][l][3]) == False):
                    print("Failed")
                    exit()

print("Success")

start, end = findMinMax(executionTimes, numRequests, numNodes)
execTime = sumExecutionTimes(executionTimes, numRequests, numNodes)
throughput = float(execTime/(end - start))
print("Throughput: %f" % (throughput))

print("Message Complexity: %f" % (np.average(messageComplexity)))
print("Response time: %f" % (np.average(responseTime)))

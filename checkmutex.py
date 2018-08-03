import numpy as np

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

f = open("config.txt", 'r')
if (f.closed):
    print("File not opened")
    exti(1)
s = f.readline()
s = s.split(" ")
numNodes = int(s[0])
numRequests = int(s[3])
f.close()

executionTimes = np.zeros((numNodes, numRequests, 4))

for i in range(0, numNodes):
    f = open("node%doutput" % (i))
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

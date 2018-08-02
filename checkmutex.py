f = open("config.txt", 'r')
if (f.closed):
    print("File not opened")
    exti(1)
s = f.readline()
s = s.split(" ")
numNodes = int(s[0])
numRequests = int(s[3])
f.close()

executionTimes = []
executionTimes.append([])

for i in range(0, numNodes-1):
    f = open("node%doutput" % (i))
    s = map(ord, f.readline().split(" "))
    executionTimes[i] = s

print(executionTimes)
#!/usr/bin/python3
import matplotlib.pyplot as plt
import sys
import numpy as np

data = {}
with open(sys.argv[1], "r") as infile:
	lines = []
	for i in infile.readlines():
		if i!='\n':
			lines.append(float(i.strip('\n')))
	for n in range(0, len(lines), 2):
		data[int(lines[n+1])]=lines[n]

x = list(data.keys())
y = list(data.values())
p = [i>80 for i in y]
array = np.array(p)
print(np.std(array))
plt.scatter(x, y, color='g')
plt.title(f"Flow Rate ({sys.argv[1]})")
#plt.style("dark_background")
plt.xlabel("Time (30ms increments)")
plt.ylabel("Flow Rate [DP]")
plt.show()

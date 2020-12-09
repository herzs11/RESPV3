#!/usr/bin/python3
import matplotlib.pyplot as plt
import sys

data = {}
with open(sys.argv[1], "r") as infile:
	lines = []
	for i in infile.readlines():
		if i!='\n':
			lines.append(float(i.strip('\n')))
	print(len(lines))
	for n in range(0, len(lines), 2):
		data[int(lines[n+1])]=lines[n]

x = list(data.keys())
y = list(data.values())
plt.scatter(x, y, color='g')
#plt.style("dark_background")
plt.title(f"Flow Rate in one revolution ({sys.argv[1]})")
plt.xlabel("Position")
plt.ylabel("Flow Rate [DP]")
plt.show()

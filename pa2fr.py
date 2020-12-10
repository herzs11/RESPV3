#!/usr/bin/python3
from math import pi
COEFF_OF_DISCHARGE = .6
D1 = .0523
D2 = .0159
AIR_DENSITY = 1.17
data = {}

def main():
	for i in range(0,140,5):
		data[i]=[flowRate(i),0]
	for i in range(0,135,5):
        	data[i][1] = round((data[i+5][0]-data[i][0])/5, 3)
	data[135][1] = data[130][1]
	str="{"
	for i in range(0, 140, 5):
		str+="{"
		str+=f"{data[i][0]},{data[i][1]}"
		str+="},"
	str+="}"
	print(str)
def flowRate(diff):
    return round((COEFF_OF_DISCHARGE*(pi/4)*(D2**2)*(2*diff/(AIR_DENSITY*(1-(D2/D1)**4)))**.5)*100**3, 4)

main()

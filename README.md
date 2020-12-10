<h1><u>ArduinoMEGA based, electric blower, low cost ventilator</u></h1>

<h3>Description</h3>
In short, there are 4 inputs for 4 different variables concerning a patients breathing ability, as recommended by the MIT open source ventilator 
project. The code mostly consists of reading sensor inputs, calculating volume of air flow, and operating a stepper motor to open and close a valve. 
The machine has two basic operating modes:
1. Volume based: In the case of a comatose patient, the breathing cycle is determined only by the volume of the patient lungs, and a consistent 
breathing rate.
2. Assisted/Patient initiated: The inhalation is triggered by a negative pressure from the patient attempting to inhale.
</br>
</br>
<h3>Code</h3>
<b>Motor homing</b>
	The stepper motor is homed by making a full revolution of the valve, and finding the maximum and minimum pressure readings. It then moves 
again until it reaches the highest reading position, and moves the other direction until it reads the lowest, and sets that as home. 
</br>
</br>
<b>Main Loop</b>
	The main loop is responsible for calling the getFlow subroutine every 30 milliseconds, which calculates the current flow rate, adjusts the 
valve to maintain the optimum flow rate, and calculates the total volume passed to the patient in that inhale. In volume control mode, It also 
starts the exhale routine if the total volume equals the volume set by the operator, or if a failsafe time has elapsed. In assisted mode, the exhale 
is only triggered by the time calculated by the optimal breathes per minute. The main loop also listens for changes on any of the pots, and will 
adjust all the settings and variables if they are changed without blocking the code, using a state changing switch statement. 
	The exhale loop in volume mode closes the valve, and allows for exhale only in the calculated time of the exhale. In assisted mode, it 
closes the valve as well, but doesn't open it until the patient attempts to inhale, or a failsafe time elapses.
</br>
</br>
<b>Settings (per MIT recommendations)</b></br>
	Tidal Peak: Total volume of lungs 200-800 mL</br>
	Respiratory Rate: Breaths per minute 6-40</br>
	I/E Ratio: Inhale to exhale ratio 1:1 - 1:4</br>
	Trigger Sensitivity: Dip in pressure that triggers an inhale (TODO!)</br>
	</br>
<b>Variables (recalculated each time settings are changed)</b></br>
	Period: Total time of one breath (millis): 60000/respRate</br>
	Tin: Time of inhale: (period/IEratio+1)x.8     <i>(80% is the breath, 20% is the holding time at the top of the inhale)</i></br>
	Thold: Holding time at the peak of inspiration: (period/IEratio+1)x.2</br>
	Tex: Time of exhale: period-Tin-Thold</br>
	flowDesired: Optimal flow rate: tidalPeak/Tin</br>
	
<h3>Demo</h3>
<p>https://youtu.be/gfGuD7pToNI</p>


// Simulated home temperature
//
//	Set is in slot of 1 hour each from 0 to 23, vector goes from 1 to 24.
//	Simulation is in slot of 1 minute each from 0 to 1440, vector goes from 1 to 1441.
//	Measured values: heating +0.15°C/hr, not heating -0.04°C/hr

function home_temperature(t0, db, setpoint)
	//Simplified model
	hon = 0.15/60;				// °C/min while heating
	hoff=-0.04/60;				// °C/min while not heating 

	//Control
	temperature=zeros(1,length(setpoint));	// °C simulated temperature
	control=zeros(1,length(setpoint));		// ON/OFF heating control

	// Simulated temperature
	temperature(1)=t0;
	control(1)=0;
	for i = 2 : length(setpoint)
		// Turn the heating on/off
		if((temperature(i-1) > (setpoint(i-1)-db)) & (temperature(i-1) < (setpoint(i-1)+db))) then
			control(i)=control(i-1);	// Keep as it is
		elseif((setpoint(i-1)-db)>(temperature(i-1))) then
			control(i)=1;				// Heat
		elseif((setpoint(i-1)+db)<(temperature(i-1))) then
			control(i)=0;				// Do not heat
		else
			control(i)=control(i-1);	// Keep as it is
		end
		
		// Simulate the temperature
		if(control(i)==1) then
			temperature(i) = temperature(i-1) + hon;
		else
			temperature(i) = temperature(i-1) + hoff;
		end
	end

	// Coun the running hours
	t_on = 0;							// hrs heating time
	for i = 1 : length(setpoint)
		t_on = t_on + control(i);
	end
	t_on=t_on/60;						// Convert minutes in hours

	// Create a time reference to plot
	time=zeros(1,length(setpoint));
	for i = 1 : length(setpoint)
		time(i)=int(i)/60;
	end

	// Plot the result
	plot(time,temperature,time,control+max(temperature)-1,time,setpoint);
	xtitle('Temperature simulation, running hours '+ string(t_on), 'hours', 'temperature (°C), control (ON/OFF)');
	xgrid;
endfunction

// Setpoint
t0=20;		 				// °C
db=0.2;						// °C deadband
setpoint=ones(1,24*60)*t0;	// °C base setpoint
eco=3;						// °C lowered setpoint in eco mode

// Build the setpoint for a day
k=1;
for i = 1 : 24
	for j = 1 : 60
		k=k+1;
		// eco mode hours
		if((i>0) & (i<6)) | ((i>8) & (i<17)) | ((i>21) & (i<24)) then
			setpoint(k)=setpoint(k)-eco;
		end
	end 
end

// Build a 6 days simulation
setpoint=[setpoint,setpoint,setpoint,setpoint,setpoint,setpoint];

// Run the simulation
home_temperature(t0, db, setpoint);

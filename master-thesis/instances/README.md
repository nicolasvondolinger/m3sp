
Each instance contains a set, of size |L|, of connections. Each connection is represented by its sender and its receiver. First, |L| receivers are randomly positioned in a Euclidean plane; then, for each receiver, a sender is randomly positioned within 6 * sqrt(2) meters from the receiver. The area of the Euclidean plane is represented by D**YYYxZZZ**, where **YYYxZZZ** are the dimensions (in meters).

The instance file is organized as follows:
+ The first line contains the values of (in order):  
		- Number of connections;
		- Alfa;
		- Noise;
		- Power transmission rate;
		- Number of Spectrums and its respective values.
- The next |L| lines contains the position of sender i in the Euclidean plane;
- Then, the position of each receiver is described in the following |L| lines;
- The gamma values are represented in the |L| lines after the last receiver position;
- The Data-rates and SINR values, according to the specifications of the Wi-Fi 6, are described in the last lines, respectively.
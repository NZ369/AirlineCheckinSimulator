# Airline Check-in Simulator

A simulation of an airline check in system using concurrent programming, implemented with POSIX pthread library.  

This simulation is of an airline check-in system, called ACS.  The check-in system includes 2 queues and 4 clerks. One queue (Queue 0) for economy class and the other (Queue 1) for business class. The customers in each queue are served FIFO, and the customers in the business class have a higher priority than the customers in the economy class. In other words, when a clerk is available, the clerk picks a customer in the business class, if any, to serve, and picks a customer in the economy class to serve only if the business class queue is empty. When a clerk is available and there is no customer in any queue, the clerk remains idle. The service time for a customer is known when the customer enters the system.  Threads are used to simulate the customers arriving and waiting for service, and the program will schedule these customers.

********************************************************************************************************************************************
********************************************************************************************************************************************

### Procedures for Running Airline Check-in System Simulator

*[Note: ACS code runs on linux environment]*

1. To compile code: On terminal execute `make` command.
2. Ensure customer entry text files are present within the same directory/folder.
3. Ensure the supplied sample input.txt file is within the same directory.
4. To run ACS executable, on terminal execute command as follows: `./ACS input.txt` command.

*[Note: input.txt can be replaced with any other customer entry text file name.]*

### The customer entry text file must be in the following format:

The input file is a text file and has a simple format. The first line contains the total number of customers that will
be simulated. After that, each line contains the information about a single customer, such that:
1. The first character specifies the unique ID of customers.
2. A colon(:) immediately follows the unique number of the customer.
3. Immediately following is an integer equal to either 1 (indicating the customer belongs to business class) or 0 (indicating the customer belongs to economy class).
4. A comma(,) immediately follows the previous number.
5. Immediately following is an integer that indicates the arrival time of the customer.
6. A comma(,) immediately follows the previous number.
7. Immediately following is an integer that indicates the service time of the customer.
8. A newline (\n) ends a line.

*[Note: input.txt contains an example of entry text file format]*

********************************************************************************************************************************************
********************************************************************************************************************************************

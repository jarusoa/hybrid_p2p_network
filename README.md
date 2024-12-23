# Distributed Resource Sharing Using a Hybrid P2P Network

## Objective
This project implements a hybrid peer-to-peer (P2P) network system for distributed resource sharing. It combines centralized server coordination with direct client-to-client communication, facilitating resource announcements, queries, and file transfers. The system supports scalability, efficient resource sharing, and fault-tolerant communication.

### Skills Learned
- Building hybrid P2P networks with centralized and decentralized components.
- Implementing multithreading for handling concurrent client requests.
- Utilizing Berkeley sockets for TCP and UDP communication.
- Designing dynamic data structures for user and resource directories.
- Integrating client liveness checks and non-blocking file transfers.

### Tools Used
- **C Programming Language**: For efficient low-level control of network operations.
- **Berkeley Sockets**: To implement TCP and UDP communication.
- **POSIX Threads**: For multithreading and synchronization.
- **Makefile**: For compiling and managing the project build process.

## Steps
### Running the Application
### Compile the program:
- make
### Start the server:
- ./server
### Start the client (replace <server_ip> and <username> with appropriate values):
- ./client <server_ip> <username>
Follow the on-screen prompts to register with the server, announce resources, query resources/users, and download files.

## Example Screenshots

### Server Running and Server Logs
This screenshot shows the server running and logs, resource announcements, and liveness checks.
![image](https://github.com/user-attachments/assets/b1b40643-73da-4b60-89f9-25a369ca9f2f)


### Resource Announcement
Here, a client announces resources, and the server updates its directory accordingly.
![image](https://github.com/user-attachments/assets/d62ded42-d9cf-4488-ad1a-31f5c80587d7)


### Query Resources
This screenshot shows a client querying the server for available resources.
![image](https://github.com/user-attachments/assets/37cf87f1-6381-4fd0-abe6-c30b1f7a223c)


### File Download
This console output captures a client downloading a file directly from another client via TCP.
This program also takes into account that different clients may have a resource named the same, yet 
different files, so it allows the user to choose who to download from if thats the case.

```bash
--- MENU ---
1. Announce a resource
2. Query available resources
3. Query active users
4. Download a resource
5. Exit
Select an option: 4
Resources:
Capture.PNG (Owner: Bob, IP: 192.168.1.200, TCP Port: 55211)
MSP2PIN.pdf (Owner: Bob, IP: 192.168.1.200, TCP Port: 55211)
test.txt (Owner: Bob, IP: 192.168.1.200, TCP Port: 55211)
cool_stuff.txt (Owner: Sally, IP: 192.168.1.109, TCP Port: 62102)
deadlift.mp4 (Owner: Sally, IP: 192.168.1.109, TCP Port: 62102)
test.txt (Owner: Sally, IP: 192.168.1.109, TCP Port: 62102)
test.py (Owner: Charlie, IP: 192.168.1.200, TCP Port: 55215)
Enter the name of the resource to download: test.txt
Available owners for resource 'test.txt':
1. Bob 192.168.1.200 55211
2. Sally 192.168.1.109 62102
Select an owner (1-2): 2
Downloading resource 'test.txt' from Sally (192.168.1.109:62102)
Resource 'test.txt' downloaded and saved as 'downloaded_Sally_test.txt'

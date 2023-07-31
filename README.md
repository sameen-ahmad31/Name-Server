# Name Server
I developed a Name Server program in C, serving as a crucial intermediary enabling clients to communicate with various servers and services. The Name Server earned its name because each service is associated with a unique string-based name. When a client seeks to interact with a particular service, it does so by requesting communication with the service, identifying it by its name. The primary responsibility of the nameserver is to establish a connection between the client and the corresponding service based on the requested name.

For instance, consider a scenario where a client wishes to inquire about the Wi-Fi strength on their system. To achieve this, the client would communicate with the service identified by the name "network_monitor." The analogy of a browser and DNS comes to mind, where entering a human-readable address in the browser's address bar leads to the conversion of that address into a numeric IP address to identify the server.

Three categories of software construct this system:
<br>
1. Clients - Utilizing "req" (built using req.c) as a representative client. "req" operates in a pipeline fashion, forwarding data read from its standard input to the server and writing the received server response to its standard output

2. Servers - Each server is associated with a concise string name. Servers are straightforward, reading client requests from their standard input and providing replies through their standard output

3. The nameserver - This component takes center stage. It receives client requests seeking specific servers identified by their names. The nameserver routes these requests to the corresponding servers, receives their replies, and returns them to the clients. Communication between the nameserver and servers relies on pipes, while domain sockets enable communication between the nameserver and clients to establish per-client channels. To handle concurrent clients effectively, the nameserver employs "poll" to manage communication efficiently

The Name Server program is designed with robust error handling and scalability in mind, allowing it to handle a large number of client requests concurrently. Furthermore, it employs data structures and algorithms to ensure fast and efficient name resolution, minimizing any potential blockages. Overall, the Name Server program enables seamless and dynamic communication between clients and multiple servers, streamlining the process of accessing various services within the system.

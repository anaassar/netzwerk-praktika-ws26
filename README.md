# netzwerk-praktika-ws26

### compile
sh bootstrap
./configure
make clean
make


### run
sudo ./echo_server → el servidor intenta reservar el puerto 7, pero Linux normalmente requiere privilegios especiales para utilizar puertos inferiores a 1024
./echo_client_connected 127.0.0.1
./echo_client 127.0.0.1

---

grep -nE 'INADDR_LOOPBACK|INADDR_ANY' echo_server.c
grep -nE 'Socket|Bind|Sendto|Recvfrom|Close' echo_client.c echo_server.c
grep -n 'MESSAGE_SIZE' echo_client.c echo_client_connected.c
para pruebas de bytes (guardar y compilar): gcc -Wall -std=c99 -pedantic -o echo_client_test echo_client_test.c Socket.c

---

### 2. Fugen Sie einen weiteren Client (basierend auf der vorherigen Aufgabe) zum Projekt hinzu, der einen connected UDP socket benutzt. Welche Auswirkungen hat dies?*
The connected UDP client uses `connect()` to associate the socket with a specific remote IP address and port. Consequently, `send()` and `recv()` can be used instead of `sendto()` and `recvfrom()`, simplifying the code. The socket normally receives datagrams only from the configured peer. However, UDP remains connectionless at the transport protocol level: `connect()` does not establish a handshake or guarantee reliable delivery. Both clients successfully received the 9216-byte echo message in our test.

### 3. Was passiert, wenn der Client sehr große Pakete schicken will? Warum?*
When sending very large messages using UDP, the transmission may fail if the datagram exceeds the maximum size supported by IPv4 or the system. In our experiment, messages of 9,216 and 60,000 bytes were successfully echoed through localhost. However, sending 65,508 bytes resulted in the error `Message too long`, because the datagram exceeded the supported size limit.

Large UDP datagrams may also require IP fragmentation when they exceed the network MTU. If a fragment is lost, the complete datagram cannot be reassembled and delivered to the application. Unlike TCP, UDP does not provide automatic retransmission or reliable delivery.


### 4. Stellen Sie die Kommunikation auf UDP-Lite um. Erweitern Sie den Client und den Server, so dass die Send und Receive Checksum Coverage uber Kommandozeilenargumente konfigurierbar sind. Testen Sie wesentliche Kombinationen dieser Parameter.

Im Rahmen dieser Aufgabe wurde das ursprüngliche UDP-Echo-Programm so angepasst, dass es das Protokoll UDP-Lite verwendet. Dazu wurde beim Stellen der Sockets das Protokoll `IPPROTO_UDPLITE` angegeben.

Mit den Socket-Optionen `UDPLITE_SEND_CSCOV` und `UDPLITE_RECV_CSCOV` kann die Checksum-Coverage beim Senden und Empfangen konfiguriert werden. Die Werte werden über Kommandozeilenargumente übergeben, sodass unterschiedliche Einstellungen ohne Änderungen am Quellcode getestet werden können.

The Server empfängt Datagramme and sendet sie anschließend an den jeweiligen Client zurück. Mit `getsockopt()` wird überprüft, welcher Wert für die Empfangs-Coverage tatsächlich gespeichert wurde.

Bei den Tests mit einer Empfangs-Coverage von 8 wurden Datagramme mit den Send-Coverage-Werten 100, 9216 und 0 erfolgreich empfangen. Beier Empfangs-Coverage von 0 wurde dagegen and in abweichendes Verhalten beobachtet: Datagramme mit vollständiger Coverage wurden empfangen, während ein Test mit partieller Coverage no Antwort lieferte.

These Ergebnis entspricht nicht unmittelbar dem erwarteten Verhalten der Socket-Option und sollte daher als Beobachtung der Testumgebung dokumentiert werden. Die Ursache konnte anhand der bisherigen Tests noch nicht eindeutig bestimmt werden.

Server	Client 	   Result
0 8	    100   0	   Received
0 8	    9216  0	   Received
0 8	    0     0	   Received
0 0	    100   0	   No answer
0 0	    0     0	   Received
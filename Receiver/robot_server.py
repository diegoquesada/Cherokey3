""" robot_server.py
Script to test telemetry sent by the robot
Copyright (c) 2026 Diego Quesada
All rights reserved.

This software is licensed under terms that can be found in the LICENSE file
in the root directory of this software component.
If no LICENSE file comes with this software, it is provided AS-IS.
"""

import socket
import sys

def robotServer():
	"""Handles connections from the robot over TCP."""
	serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	serverSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	serverSocket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

	try:
		serverSocket.bind(('0.0.0.0', 9576))
	except socket.error as e:
		print(f"Error binding socket: {e}", file=sys.stderr)
		sys.exit(1)
		
	serverSocket.listen(1)
	serverSocket.settimeout(60) # Default 1 minute to avoid hanging on bad bind
	print("Echo server listening on port 9576...")

	try:
		while True:
			try:
				clientSocket, clientAddress = serverSocket.accept()
				print(f"Connected by {clientAddress}")

				while True:		
					data = clientSocket.recv(1024)
					if not data:
						# Client disconnected (EOF)
						print(f"Client {clientAddress} disconnected.", file=sys.stderr)
						break
					else:
						# Decode and print data
						try:
							text = data.decode('utf-8', errors='replace')
							sys.stdout.write(text)
							sys.stdout.flush()
						except AttributeError:
							# Fallback if decoding fails
							sys.stdout.write(data.decode('latin-1', errors='replace'))
							sys.stdout.flush()
						
			except socket.timeout:
				continue # Data not ready yet
			except socket.error:
				print("Socket error during accept.", file=sys.stderr)
				break
			except ConnectionResetError:
				print(f"Cilent {clientAddress} reset connection.", file=sys.stderr)
				break
			except Exception as e:
				print(f"Unhandled exception: {e}", file=sys.stderr)
				break
			
	finally:
		# Cleanup
		serverSocket.close()
		print("Client disconnected.")

if __name__ == "__main__":
	try:
		robotServer()
	except KeyboardInterrupt:
		print ("\nServer stopped.");
	except Exception as e:
		print(f"Error: {e}")


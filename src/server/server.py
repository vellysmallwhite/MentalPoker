# server.py
import socket
import threading
import json
from collections import defaultdict
import time
import uuid
import os

class PokerServer:
    def __init__(self, port=8080):
        self.host = os.getenv('HOSTNAME', 'poker-server.local')
        self.port = port
        self.rooms = {}
        self.lock = threading.Lock()
        self.pending_joins = {}
        self.acks = defaultdict(set)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.debug = True  # Debug flag

    def debug_print(self, message):
        if self.debug:
            print(f"[DEBUG] {message}")

    def start(self):
        self.sock.bind((self.host, self.port))
        self.sock.listen(5)
        self.debug_print(f"Server listening on {self.host}:{self.port}")
        try:
            while True:
                client, addr = self.sock.accept()
                self.debug_print(f"New connection from {addr[0]}:{addr[1]}")
                thread = threading.Thread(target=self.handle_client, args=(client, addr),daemon=True)
                thread.start()
        except KeyboardInterrupt:
            self.debug_print("Server shutting down.")
        finally:
            self.sock.close()

    def handle_client(self, client, addr):
        client.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        try:
            while True:
                data = self.receive_message(client)
                if data is None:
                    #self.debug_print(f"Client  sent no data.")
                    continue  # Exit the loop and close the connection

                try:
                    message = json.loads(data)
                except json.JSONDecodeError:
                    self.debug_print(f"Received invalid JSON from {addr}: {data}")
                    self.send_message(client, {'status': 'error', 'message': 'Invalid JSON'})
                    continue  # Continue to the next iteration

                client_id = message.get('client_id', 'Unknown')
                self.debug_print(f"Received message from {client_id}: {message}")
                response = self.process_message(message)

                try:
                    self.send_message(client, response)
                except (BrokenPipeError, ConnectionResetError) as e:
                    self.debug_print(f"Error sending to {addr}: {e}")
                    break  # Exit the loop and close the connection
        except Exception as e:
            self.debug_print(f"Unexpected error with client {addr}: {e}")
        finally:
            client.close()
            self.debug_print(f"Closed connection with {addr}")

    def receive_message(self, client):
        try:
            length_bytes = self.recv_all(client, 4)
            if not length_bytes:
                return None  # Client disconnected
            length = int.from_bytes(length_bytes, 'big')
            if length == 0:
                return None
            data = self.recv_all(client, length)
            if not data:
                return None
            return data.decode('utf-8')
        except socket.error as e:
            self.debug_print(f"Socket error during receive: {e}")
            return None

    def recv_all(self, client, n):
        """Helper function to receive exactly n bytes"""
        data = b''
        while len(data) < n:
            packet = client.recv(n - len(data))
            if not packet:
                return None
            data += packet
        return data

    def send_message(self, client, message):
        try:
            data = json.dumps(message).encode('utf-8')
            length = len(data).to_bytes(4, byteorder='big')
            client.sendall(length + data)
        except socket.error as e:
            self.debug_print(f"Socket error during send: {e}")
            raise

    def process_message(self, message):
        cmd = message.get('command')
        room_id = message.get('room_id')
        client_id = message.get('client_id')

        try:
            if cmd == 'JOIN':
                self.debug_print(f"Handling JOIN command from client {client_id}")
                response = self.handle_join(room_id, client_id)
                self.debug_print(f"JOIN response: {response}")
                return response
            elif cmd == 'JOIN_ACK':
                self.debug_print(f"Handling JOIN_ACK from client {client_id}")
                self.handle_join_ack(client_id, message.get('room_id'))
                return {'status': 'ack_received'}
            

            elif cmd == 'LIST':
                self.debug_print(f"Handling LIST command from client {client_id}")
                response = self.handle_list(room_id)
                self.debug_print(f"LIST response: {response}")
                return response

            elif cmd == 'LEAVE':
                self.debug_print(f"Handling LEAVE command from client {client_id}")
                response = self.handle_leave(room_id, client_id)
                self.debug_print(f"LEAVE response: {response}")
                return response

            else:
                error_msg = f"Unknown command: {cmd}"
                self.debug_print(error_msg)
                return {'status': 'error', 'message': error_msg}

        except Exception as e:
            self.debug_print(f"Error processing command: {e}")
            return {'status': 'error', 'message': str(e)}

    def handle_join(self, room_id, client_id):
            # Case 1: New Room
            if room_id not in self.rooms:
                self.rooms[room_id] = {
                    'status': 'available',
                    'members': [{
                        'id': client_id,
                        'hostname': f"{client_id}"
                    }],
                    'created_at': time.time()
                }
                return {
                    'status': 'success',
                    'message': 'Created new room',
                    'room_id': room_id,
                    'members': []
                }

            # Case 2: Existing Room
            room = self.rooms[room_id]
            if room['status'] == 'busy':
                return {
                    'status': 'error',
                    'message': 'Room is busy'
                }

            # Get current members
            current_members = [m['hostname'] for m in room['members']]
            
            # Mark room as busy during join process
            room['status'] = 'busy'
            
            # Create pending join operation
            join_id = str(uuid.uuid4())
            self.pending_joins[join_id] = {
                'room_id': room_id,
                'client_id': client_id,
                'created_at': time.time()
            }

            return {
                'status': 'pending',
                'join_id': join_id,
                'message': 'Waiting for member acknowledgments',
                'members': current_members
            }


    def handle_list(self, room_id):
        with self.lock:
            members = self.rooms.get(room_id, {}).get('members', [])
        return {'status': 'success', 'members': members}

    def handle_leave(self, room_id, client_id):
        with self.lock:
            if room_id in self.rooms:
                original_count = len(self.rooms[room_id]['members'])
                self.rooms[room_id]['members'] = [
                    m for m in self.rooms[room_id]['members'] if m['hostname'] != client_id
                ]
                if len(self.rooms[room_id]['members']) < original_count:
                    return {'status': 'success', 'message': f"{client_id} left {room_id}"}
                else:
                    return {'status': 'error', 'message': f"{client_id} was not in {room_id}"}
            else:
                return {'status': 'error', 'message': f"Room {room_id} does not exist"}

    def handle_client_disconnect(self, client_id):
        with self.lock:
            for room in self.rooms.values():
                room['members'] = [m for m in room['members'] if m['hostname'] != client_id]
        self.debug_print(f"Cleaned up client {client_id}")

# Usage
if __name__ == "__main__":
    server = PokerServer(port=8080)
    server.start()
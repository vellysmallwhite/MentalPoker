# server.py
import socket
import threading
import json
from collections import defaultdict
import time
import uuid

class PokerServer:
    def __init__(self, port=8080):
        self.host = os.getenv('HOSTNAME','poker-server.local')
        self.port = port
        self.rooms = {}
        self.lock = threading.Lock()
        self.pending_joins = {}  # Track pending join operations
        self.acks = defaultdict(set)  # Track acknowledgments
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
    def start(self):
        self.sock.bind((self.host, self.port))
        self.sock.listen(5)
        print(f"Server listening on {self.host}:{self.port}")
        
        while True:
            client, addr = self.sock.accept()
            thread = threading.Thread(target=self.handle_client, args=(client,))
            thread.start()
            
    def handle_client(self, client):
        try:
            while True:
                data = self.receive_message(client)
                if not data:
                    break
                    
                message = json.loads(data)
                response = self.process_message(message)
                self.send_message(client, response)
                
        except Exception as e:
            print(f"Error handling client: {e}")
        finally:
            client.close()
            
    def process_message(self, message):
        with self.lock:
            cmd = message.get('command')
            room_id = message.get('room_id')
            client_id = message.get('client_id')
            
            if cmd == 'JOIN':
                return self.handle_join(room_id, client_id)
            elif cmd == 'LIST':
                return self.handle_list(room_id)
            elif cmd == 'JOIN_ACK':
                join_id = message.get('join_id')
                member_id = message.get('member_id')
                return self.handle_join_ack(join_id, member_id)
                
        return {'status': 'error', 'message': 'Invalid command'}
        
    def handle_join(self, room_id, client_id):
        with self.lock:
            # Case 1: New Room
            if room_id not in self.rooms:
                self.rooms[room_id] = {
                    'status': 'available',
                    'members': [{
                        'id': client_id,
                        'hostname': f"player{client_id}.local"
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

    def handle_join_ack(self, join_id, member_id):
        with self.lock:
            if join_id not in self.pending_joins:
                return {'status': 'error', 'message': 'Invalid join operation'}

            join_op = self.pending_joins[join_id]
            self.acks[join_id].add(member_id)

            # Check if we have enough acknowledgments
            if len(self.acks[join_id]) >= len(room['members']) // 2:
                room = self.rooms[join_op['room_id']]
                room['members'].append({
                    'id': join_op['client_id'],
                    'hostname': f"player{join_op['client_id']}.local"
                })
                room['status'] = 'available'
                
                # Cleanup
                del self.pending_joins[join_id]
                del self.acks[join_id]

                return {
                    'status': 'success',
                    'message': 'Join operation completed'
                }

            return {
                'status': 'pending',
                'message': 'Waiting for more acknowledgments'
            }

    def receive_message(self, client):
        length_bytes = client.recv(4)
        if not length_bytes:
            return None
        length = int.from_bytes(length_bytes, 'big')
        return client.recv(length).decode('utf-8')
        
    def send_message(self, client, message):
        data = json.dumps(message).encode('utf-8')
        length = len(data).to_bytes(4, 'big')
        client.send(length + data)

if __name__ == '__main__':
    server = PokerServer()
    server.start()
import socketio

s = socketio.Client()
s.connect('ws://localhost:9000')
s.emit('door-connected', 'door 02')
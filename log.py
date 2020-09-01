import socketio

s = socketio.Client()
s.connect('ws://localhost:9000')
s.emit('user-log', {'id': '1', 'direction': 'OUT'})

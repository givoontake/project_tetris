from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit

app = Flask(__name__)
socketio = SocketIO(app)
received_message = ""

@app.route('/')
def index():
    return render_template('main.html')

@app.route('/main', methods=['POST'])
def connect():
    # html에서 json 데이터를 수신
    try:
        data = request.get_json()
        if not data:
            return jsonify({"status": "error", "message": "No data received"}), 400

        text = data.get('message', '')
        if not text:
            return jsonify({"status": "error", "message": "No 'message' field"}), 400

        # 받은 데이터를 바로 Socket.IO(로 전송
        socketio.emit('new_message', {'message': text})
        # print("전송된 메시지:", text)

        return jsonify({"status": "success", "message": f"Sent: {text}"})

    except Exception as e:
        print("에러 발생:", e)
        return jsonify({"status": "error", "message": str(e)}), 500
    

@app.route('/receive', methods=['POST'])
def receive():
    try:
        global received_message
        received_message = request.data.decode('utf-8')
        print({'received_message': received_message})
        socketio.emit('new_message', {'message': received_message})
        return jsonify({"status": "success", "message": received_message})
    except Exception as e:
        print("에러 발생:", e)
        return jsonify({"status": "error", "message": str(e)}), 500

if __name__ == '__main__':
    socketio.run(app, host='127.0.0.1', port=5000, debug=True)

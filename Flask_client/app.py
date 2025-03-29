from flask import Flask, render_template, request, jsonify, url_for, redirect, session
from flask_socketio import SocketIO, emit

app = Flask(__name__)
app.secret_key = 'my_secret_key'  # session 사용을 위한 필수 설정
socketio = SocketIO(app)
received_message = ""

@app.before_request
def set_default_session():
    if 'user_id' not in session:
        session['user_id'] = 'guest'

@app.route('/')
def index():
    return render_template('lobby.html')

@app.route('/lobby_find')
def lobby_find():
    return render_template('lobby_find.html')

@app.route('/lobby_make')
def lobby_make():
    return render_template('lobby_make.html')

@app.route('/main', methods=['POST'])
def chatting():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"status": "error", "message": "No data received"}), 400

        text = data.get('message', '')
        if not text:
            return jsonify({"status": "error", "message": "No 'message' field"}), 400

        # 받은 메시지를 소켓으로 전송
        socketio.emit('new_message', {'message': text})

        return jsonify({"status": "success", "message": f"Sent: {text}"})

    except Exception as e:
        print("에러 발생:", e)
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/lobby')
def lobby():
    # 세션에서 user_id 사용 → 템플릿에서 {{ session.user_id }}로 접근 가능
    return render_template("lobby.html")

@app.route('/receive', methods=['POST'])
def receive():
    try:
        data = request.get_json()
        msg_type = data.get("type")

        if msg_type == "s2c_login":
            session['user_id'] = data.get("id", "")
            return redirect(url_for('lobby'))

        elif msg_type == "s2c_message":
            message = data.get("message", "")
            return render_template('main.html', message=message)

        else:
            return jsonify({"status": "error", "message": f"알 수 없는 타입: {msg_type}"}), 400

    except Exception as e:
        print("에러 발생:", e)
        return jsonify({"status": "error", "message": str(e)}), 500

if __name__ == '__main__':
    socketio.run(app, host='127.0.0.1', port=5000, debug=True)

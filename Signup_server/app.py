import re
import os
import hmac
import time
import bcrypt
import smtplib
import random
from email.mime.text import MIMEText
from flask import Flask, render_template, request, jsonify, session

from config import Config
from models import db, User

def create_app():
    app = Flask(__name__)
    app.config.from_object(Config)
    db.init_app(app)

    with app.app_context():
        db.create_all()

    # ---------------------------
    # 유틸: 비밀번호 규칙 체크
    # ---------------------------
    def validate_password(pw: str) -> bool:
        # 8~20, 영문/숫자/특수문자 중 2종 이상 (너가 원하는 규칙으로 쉽게 변경 가능)
        if not (8 <= len(pw) <= 20):
            return False

        has_alpha = bool(re.search(r"[A-Za-z]", pw))
        has_digit = bool(re.search(r"\d", pw))
        has_spec  = bool(re.search(r"[^\w]", pw))  # 특수문자

        kinds = sum([has_alpha, has_digit, has_spec])
        return kinds >= 2

    def send_email_code(to_email: str, code: str):
        host = app.config["SMTP_HOST"]
        port = app.config["SMTP_PORT"]
        user = app.config["SMTP_USER"]
        pw   = app.config["SMTP_PASS"]
        from_email = app.config["SMTP_FROM"]

        subject = "[Tetris] 이메일 인증번호"
        body = f"인증번호는 [{code}] 입니다. (유효시간 {app.config['EMAIL_CODE_TTL']//60}분)"

        msg = MIMEText(body, _charset="utf-8")
        msg["Subject"] = subject
        msg["From"] = from_email
        msg["To"] = to_email

        server = smtplib.SMTP(host, port)
        server.starttls()
        server.login(user, pw)
        server.sendmail(from_email, [to_email], msg.as_string())
        server.quit()

    def now_ts() -> int:
        return int(time.time())

    # ---------------------------
    # 페이지
    # ---------------------------
    @app.get("/")
    @app.get("/signup")
    def signup_page():
        return render_template("signup.html")

    # ---------------------------
    # API: 아이디 중복 체크
    # ---------------------------
    @app.post("/api/check-id")
    def api_check_id():
        login_id = (request.json or {}).get("login_id", "").strip()

        if not (6 <= len(login_id) <= 20) or not re.match(r"^[A-Za-z0-9_]+$", login_id):
            return jsonify(ok=False, msg="아이디는 6~20자, 영문/숫자/언더바(_)만 가능합니다."), 400

        exists = db.session.query(User.user_id).filter_by(login_id=login_id).first() is not None
        if exists:
            return jsonify(ok=False, msg="이미 사용 중인 아이디입니다."), 200
        return jsonify(ok=True, msg="사용 가능한 아이디입니다."), 200
    
    @app.post("/api/check-nickname")
    def api_check_nickname():
        nickname = (request.json or {}).get("nickname", "").strip()

        # 예시 규칙: 2~20자, 한글/영문/숫자/언더바 허용
        if not (2 <= len(nickname) <= 20) or not re.match(r"^[A-Za-z0-9가-힣_]+$", nickname):
            return jsonify(ok=False, msg="닉네임은 2~20자, 한글/영문/숫자/언더바(_)만 가능합니다."), 400

        exists = db.session.query(User.user_id).filter_by(nickname=nickname).first() is not None
        if exists:
            return jsonify(ok=False, msg="이미 사용 중인 닉네임입니다."), 200
        return jsonify(ok=True, msg="사용 가능한 닉네임입니다."), 200


    # ---------------------------
    # API: 인증번호 발송
    # ---------------------------
    @app.post("/api/send-code")
    def api_send_code():
        data = request.json or {}
        email = data.get("email", "").strip()

        # 매우 간단한 이메일 형식 체크
        if not re.match(r"^[^@]+@[^@]+\.[^@]+$", email):
            return jsonify(ok=False, msg="이메일 형식이 올바르지 않습니다."), 400
        
        if db.session.query(User.user_id).filter_by(email=email).first():
            return jsonify(ok=False, msg="이미 가입된 이메일입니다."), 200

        code = f"{random.randint(0, 999999):06d}"
        ttl = app.config["EMAIL_CODE_TTL"]

        # 코드 저장: 세션에 "해시"로 저장 (원문 저장 X)
        code_hash = bcrypt.hashpw(code.encode(), bcrypt.gensalt()).decode()

        session["email_verify"] = {
            "email": email,
            "code_hash": code_hash,
            "expire_at": now_ts() + ttl,
            "verified": False
        }

        try:
            send_email_code(email, code)
        except Exception as e:
            return jsonify(ok=False, msg=f"이메일 발송 실패: {str(e)}"), 500

        return jsonify(ok=True, msg="인증번호를 발송했습니다. 이메일을 확인하세요."), 200

    # ---------------------------
    # API: 인증번호 검증
    # ---------------------------
    @app.post("/api/verify-code")
    def api_verify_code():
        data = request.json or {}
        email = data.get("email", "").strip()
        code = data.get("code", "").strip()

        st = session.get("email_verify")
        if not st:
            return jsonify(ok=False, msg="인증번호 발송부터 진행하세요."), 400

        if st.get("email") != email:
            return jsonify(ok=False, msg="인증 요청 이메일이 일치하지 않습니다."), 400

        if now_ts() > int(st.get("expire_at", 0)):
            return jsonify(ok=False, msg="인증번호가 만료되었습니다. 다시 발송하세요."), 400

        code_hash = st.get("code_hash", "")
        if not code_hash or not bcrypt.checkpw(code.encode(), code_hash.encode()):
            return jsonify(ok=False, msg="인증번호가 올바르지 않습니다."), 200

        st["verified"] = True
        session["email_verify"] = st
        return jsonify(ok=True, msg="이메일 인증이 완료되었습니다."), 200

    # ---------------------------
    # API: 회원가입 완료
    # ---------------------------
    @app.post("/api/register")
    def api_register():
        data = request.json or {}
        login_id = data.get("login_id", "").strip()
        nickname = data.get("nickname", "").strip()
        password = data.get("password", "")
        password2 = data.get("password2", "")
        email = data.get("email", "").strip()

        # 1) 아이디 형식
        if not (6 <= len(login_id) <= 20) or not re.match(r"^[A-Za-z0-9_]+$", login_id):
            return jsonify(ok=False, msg="아이디 형식이 올바르지 않습니다."), 400

        # 2) 비밀번호 체크
        if password != password2:
            return jsonify(ok=False, msg="비밀번호가 일치하지 않습니다."), 400
        if not validate_password(password):
            return jsonify(ok=False, msg="비밀번호 규칙을 만족하지 않습니다."), 400

        # 3) 이메일 인증 확인
        st = session.get("email_verify")
        if not st or st.get("email") != email or not st.get("verified"):
            return jsonify(ok=False, msg="이메일 인증을 먼저 완료하세요."), 400
        if now_ts() > int(st.get("expire_at", 0)):
            return jsonify(ok=False, msg="이메일 인증이 만료되었습니다. 다시 인증하세요."), 400

        # 4) 중복 체크 (서버에서 최종 보장)
        if db.session.query(User.user_id).filter_by(login_id=login_id).first():
            return jsonify(ok=False, msg="이미 사용 중인 아이디입니다."), 200

        if db.session.query(User.user_id).filter_by(nickname=nickname).first():
            return jsonify(ok=False, msg="이미 사용 중인 닉네임입니다."), 200

        if db.session.query(User.user_id).filter_by(email=email).first():
            return jsonify(ok=False, msg="이미 가입된 이메일입니다."), 200

        # 5) 비밀번호 해시 저장 (bcrypt)
        pw_hash = bcrypt.hashpw(password.encode(), bcrypt.gensalt()).decode()

        # 닉네임은 일단 login_id로 자동 세팅(필요하면 폼에 추가해서 받으면 됨)
        user = User(
            login_id=login_id,
            nickname=nickname,
            password_hash=pw_hash,
            email=email,
        )
        db.session.add(user)
        db.session.commit()

        # 인증 상태 제거(재사용 방지)
        session.pop("email_verify", None)

        return jsonify(ok=True, msg="회원가입이 완료되었습니다."), 200

    return app

app = create_app()

if __name__ == "__main__":
    # 개발용 실행
    app.run(host="0.0.0.0", port=int(os.getenv("PORT", "5000")), debug=True)

function setMsg(id, text, ok) {
  const el = document.getElementById(id);
  el.textContent = text || "";
  el.className = "msg " + (ok ? "ok" : (text ? "err" : ""));
}

function pwRule(pw) {
  if (pw.length < 8 || pw.length > 20) return false;
  const hasA = /[A-Za-z]/.test(pw);
  const hasD = /\d/.test(pw);
  const hasS = /[^\w]/.test(pw);
  return (hasA + hasD + hasS) >= 2;
}

async function postJson(url, body) {
  const res = await fetch(url, {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body)
  });
  const data = await res.json().catch(() => ({}));
  return { status: res.status, data };
}

let idCheckedOk = false;
let nickCheckedOk = false;
let emailVerifiedOk = false;

document.getElementById("btn_check_id").addEventListener("click", async () => {
  const login_id = document.getElementById("login_id").value.trim();
  const { status, data } = await postJson("/api/check-id", { login_id });

  if (status === 200 && data.ok) {
    idCheckedOk = true;
    setMsg("id_msg", data.msg, true);
  } else {
    idCheckedOk = false;
    setMsg("id_msg", data.msg || "아이디 확인 실패", false);
  }
});

document.getElementById("btn_check_nick").addEventListener("click", async () => {
  const nickname = document.getElementById("nickname").value.trim();
  const { status, data } = await postJson("/api/check-nickname", { nickname });

  if (status === 200 && data.ok) {
    nickCheckedOk = true;
    setMsg("nick_msg", data.msg, true);
  } else {
    nickCheckedOk = false;
    setMsg("nick_msg", data.msg || "닉네임 확인 실패", false);
  }
});


document.getElementById("password").addEventListener("input", () => {
  const pw = document.getElementById("password").value;
  if (!pw) return setMsg("pw_msg", "", false);
  if (pwRule(pw)) setMsg("pw_msg", "사용 가능한 비밀번호입니다.", true);
  else setMsg("pw_msg", "8~20자, 영문/숫자/특수문자 중 2종 조합 필요", false);
});

document.getElementById("password2").addEventListener("input", () => {
  const pw = document.getElementById("password").value;
  const pw2 = document.getElementById("password2").value;
  if (!pw2) return setMsg("pw2_msg", "", false);
  if (pw === pw2) setMsg("pw2_msg", "비밀번호가 일치합니다.", true);
  else setMsg("pw2_msg", "비밀번호가 일치하지 않습니다.", false);
});

document.getElementById("btn_send_code").addEventListener("click", async () => {
  const email = document.getElementById("email").value.trim();
  const { status, data } = await postJson("/api/send-code", { email });

  emailVerifiedOk = false;
  if (status === 200 && data.ok) setMsg("email_msg", data.msg, true);
  else setMsg("email_msg", data.msg || "인증번호 발송 실패", false);
});

document.getElementById("btn_verify_code").addEventListener("click", async () => {
  const email = document.getElementById("email").value.trim();
  const code = document.getElementById("code").value.trim();
  const { status, data } = await postJson("/api/verify-code", { email, code });

  if (status === 200 && data.ok) {
    emailVerifiedOk = true;
    setMsg("code_msg", data.msg, true);
  } else {
    emailVerifiedOk = false;
    setMsg("code_msg", data.msg || "인증 실패", false);
  }
});

document.getElementById("btn_register").addEventListener("click", async () => {
  const login_id = document.getElementById("login_id").value.trim();
  const nickname = document.getElementById("nickname").value.trim();
  const password = document.getElementById("password").value;
  const password2 = document.getElementById("password2").value;
  const email = document.getElementById("email").value.trim();

  if (!idCheckedOk) return setMsg("id_msg", "아이디 중복 확인을 먼저 해주세요.", false);
  if (!nickCheckedOk) return setMsg("nick_msg", "닉네임 중복 확인을 먼저 해주세요.", false);
  if (!pwRule(password)) return setMsg("pw_msg", "비밀번호 규칙을 만족하지 않습니다.", false);
  if (password !== password2) return setMsg("pw2_msg", "비밀번호가 일치하지 않습니다.", false);
  if (!emailVerifiedOk) return setMsg("code_msg", "이메일 인증을 먼저 완료하세요.", false);

  const { status, data } = await postJson("/api/register", { login_id, nickname, password, password2, email });

  if (status === 200 && data.ok) {
    alert(data.msg);
    window.location.href = "/signup";
  } else {
    const msg = (data && data.msg) ? data.msg : "회원가입 실패";

    if (msg.includes("이메일")) {
      setMsg("email_msg", msg, false);
      setMsg("code_msg", "", false);
      return;
    }

    if (msg.includes("아이디")) {
      setMsg("id_msg", msg, false);
      return;
    }

    alert(msg);
  }
});

document.getElementById("btn_cancel").addEventListener("click", () => {
  window.location.href = "/signup";
});

// 아이디 입력 바뀌면 중복확인 상태 해제
document.getElementById("login_id").addEventListener("input", () => {
  idCheckedOk = false;
  setMsg("id_msg", "", false);
});

document.getElementById("nickname").addEventListener("input", () => {
  nickCheckedOk = false;
  setMsg("nick_msg", "", false);
});

// 이메일/코드 바뀌면 인증 상태 해제
document.getElementById("email").addEventListener("input", () => {
  emailVerifiedOk = false;
  setMsg("email_msg", "", false);
  setMsg("code_msg", "", false);
});
document.getElementById("code").addEventListener("input", () => {
  emailVerifiedOk = false;
  setMsg("code_msg", "", false);
});

from tests.conftest import login, register


def auth_header(token: str) -> dict[str, str]:
    return {"Authorization": f"Bearer {token}"}


# --- 회원가입 ---
def test_signup_success(client):
    res = register(client)
    assert res.status_code == 201
    body = res.json()
    assert body["email"] == "alice@example.com"
    assert body["username"] == "alice"
    assert "id" in body and "hashed_password" not in body


def test_signup_duplicate_email(client):
    register(client)
    res = register(client)
    assert res.status_code == 400


def test_signup_invalid_payload(client):
    res = client.post(
        "/auth/signup",
        json={"email": "not-an-email", "username": "x", "password": "short"},
    )
    assert res.status_code == 422


# --- 로그인 ---
def test_login_success_returns_two_tokens(client):
    register(client)
    res = login(client)
    assert res.status_code == 200
    body = res.json()
    assert body["access_token"] and body["refresh_token"]
    assert body["token_type"] == "bearer"


def test_login_wrong_password(client):
    register(client)
    res = login(client, password="wrongpassword")
    assert res.status_code == 401


# --- 보호 엔드포인트 (/auth/me) ---
def test_me_with_valid_token(client):
    register(client)
    tokens = login(client).json()
    res = client.get("/auth/me", headers=auth_header(tokens["access_token"]))
    assert res.status_code == 200
    assert res.json()["email"] == "alice@example.com"


def test_me_without_token(client):
    res = client.get("/auth/me")
    assert res.status_code in (401, 403)


def test_me_with_forged_token(client):
    res = client.get("/auth/me", headers=auth_header("garbage.token.value"))
    assert res.status_code == 401


# --- refresh 회전 ---
def test_refresh_rotates_tokens(client):
    register(client)
    tokens = login(client).json()
    res = client.post("/auth/refresh", json={"refresh_token": tokens["refresh_token"]})
    assert res.status_code == 200
    new_tokens = res.json()
    assert new_tokens["refresh_token"] != tokens["refresh_token"]

    # 회전된(기존) refresh 재사용은 거부
    reuse = client.post(
        "/auth/refresh", json={"refresh_token": tokens["refresh_token"]}
    )
    assert reuse.status_code == 401


def test_refresh_invalid_token(client):
    res = client.post("/auth/refresh", json={"refresh_token": "garbage"})
    assert res.status_code == 401


# --- 로그아웃 ---
def test_logout_revokes_refresh(client):
    register(client)
    tokens = login(client).json()
    res = client.post(
        "/auth/logout",
        json={"refresh_token": tokens["refresh_token"]},
        headers=auth_header(tokens["access_token"]),
    )
    assert res.status_code == 204

    reuse = client.post(
        "/auth/refresh", json={"refresh_token": tokens["refresh_token"]}
    )
    assert reuse.status_code == 401


# --- 탈퇴 (소프트 삭제) ---
def test_withdraw_deactivates_and_blocks_login(client):
    register(client)
    tokens = login(client).json()
    res = client.delete("/auth/me", headers=auth_header(tokens["access_token"]))
    assert res.status_code == 204

    # 재로그인 불가 (비활성)
    relogin = login(client)
    assert relogin.status_code == 401

    # 기존 refresh 무효화
    reuse = client.post(
        "/auth/refresh", json={"refresh_token": tokens["refresh_token"]}
    )
    assert reuse.status_code == 401

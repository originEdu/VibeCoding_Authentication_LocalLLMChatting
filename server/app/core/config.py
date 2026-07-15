from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    """환경 변수(.env)에서 로드하는 애플리케이션 설정."""

    database_url: str
    jwt_secret: str
    jwt_algorithm: str = "HS256"
    access_token_expire_minutes: int = 30
    refresh_token_expire_days: int = 14

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")


settings = Settings()

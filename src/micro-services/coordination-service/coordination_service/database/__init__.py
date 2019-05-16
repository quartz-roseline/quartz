from flask_sqlalchemy import SQLAlchemy

db = SQLAlchemy()


def reset_database():
    from coordination_service.database.models import Timeline, Node  # noqa
    db.drop_all()
    db.create_all()
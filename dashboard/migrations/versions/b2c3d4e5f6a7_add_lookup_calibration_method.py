"""Add as5600-lookup calibration method

Revision ID: b2c3d4e5f6a7
Revises: a1b2c3d4e5f6
Create Date: 2024-12-18 18:00:00.000000

"""
import json
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = 'b2c3d4e5f6a7'
down_revision = 'a1b2c3d4e5f6'
branch_labels = None
depends_on = None

LOOKUP_METHOD_ID = 'f8a2c9d1e4b74a8f9c3d6e5f0a1b2c3d'


def upgrade():
    conn = op.get_bind()
    properties = json.dumps({"type": "as5600-lookup"})

    conn.execute(
        sa.text("""
            INSERT INTO calibration_method (id, name, description, data, updated, client_updated)
            VALUES (:id, :name, :description, :data, unixepoch('now'), 0)
        """),
        {
            "id": LOOKUP_METHOD_ID,
            "name": "as5600-lookup",
            "description": "AS5600 lookup table with linear interpolation. Enter sensor angle (degrees) vs stroke (mm) points.",
            "data": properties
        }
    )


def downgrade():
    conn = op.get_bind()
    conn.execute(
        sa.text("DELETE FROM calibration_method WHERE id = :id"),
        {"id": LOOKUP_METHOD_ID}
    )

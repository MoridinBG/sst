"""add imu column

Revision ID: 9f1234567890
Revises: 5c40381ea62d
Create Date: 2024-12-24 10:00:00.000000

"""
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = '9f1234567890'
down_revision = '5c40381ea62d'
branch_labels = None
depends_on = None


def upgrade():
    op.add_column('session_html', sa.Column('imu', sa.String(), nullable=True))


def downgrade():
    op.drop_column('session_html', 'imu')

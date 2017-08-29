# -*- coding: utf-8 -*-

from flask_script import Manager, Shell

from rps import createAPP

app = createAPP()
manager = Manager(app)


@manager.command
def run():
    """Run in local machine."""

    app.run()

if __name__ == "__main__":
    manager.run()
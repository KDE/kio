# This file is part of KDE
# SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>
#
# SPDX-License-Identifier: LGPL-2.0-or-later

from flask import Flask
from flask import Response, redirect, request
from flask_httpauth import HTTPBasicAuth
import datetime

app = Flask(__name__)
auth = HTTPBasicAuth()

@app.route("/mime/html", methods = ['GET'])
def mime_html():
    return "<p>Hello, World!</p>"

@app.route("/mime/calendar", methods = ['GET'])
def mime_calendar():
    return Response("bla", mimetype='text/calendar')

@app.route("/get/html", methods = ['GET'])
def get_html():
    return "<p>Hello, World!</p>"

@app.route("/get/calendar", methods = ['GET'])
def get_calendar():
    data = "Some data\nthat\nhas\nnew\nlines\n"
    resp = Response(data, mimetype='text/calendar')
    return resp

# Redirection

# GET

@app.route("/get/redirected", methods = ['GET'])
def get_redirected():
    return "Itsa me, redirected\n"

@app.route("/get/permanent_redirected", methods = ['GET'])
def get_redirected_permanent():
    return "Itsa me, redirected permanently\n"

# 301
@app.route("/get/permanent_redirect", methods = ['GET'])
def get_redirect_permanent():
    return redirect("/get/permanent_redirected", code=301)

# 302
@app.route("/get/redirect", methods = ['GET'])
def get_redirect():
    return redirect("/get/redirected", code=302)

# 303
@app.route("/get/redirect_303", methods = ['GET'])
def get_redirect_303():
    return redirect('/get/redirected', code=303)

# 307
@app.route("/get/redirect_307", methods = ['GET'])
def get_redirect_307():
    return redirect('/get/redirected', code=307)

# 308
@app.route("/get/redirect_308", methods = ['GET'])
def get_redirect_308():
    return redirect('/get/permanent_redirected', code=308)

# POST

@app.route("/post/redirected", methods = ['POST'])
def post_redirected():
    return "Itsa me, redirected\n"

@app.route("/post/permanent_redirected", methods = ['POST'])
def post_redirected_permanent():
    return "Itsa me, redirected permanently\n"

# 301
@app.route("/post/permanent_redirect", methods = ['POST'])
def post_redirect_permanent():
    return redirect('/get/permanent_redirected', code=301)

# 302
@app.route("/post/redirect", methods = ['POST'])
def post_redirect():
    return redirect('/get/redirected', code=302)

# 303
@app.route("/post/redirect_303", methods = ['POST'])
def post_redirect_303():
    return redirect('/get/redirected', code=303)

# 307
@app.route("/post/redirect_307", methods = ['POST'])
def post_redirect_307():
    return redirect('/post/redirected', code=307)

# 308
@app.route("/post/redirect_308", methods = ['POST'])
def post_redirect_308():
    return redirect('/post/permanent_redirected', code=308)

# PUT

@app.route("/put/redirected", methods = ['PUT'])
def put_redirected():
    return "Itsa me, redirected\n"

@app.route("/put/permanent_redirected", methods = ['PUT'])
def put_redirected_permanent():
    return "Itsa me, redirected permanently\n"

# 301
@app.route("/put/permanent_redirect", methods = ['PUT'])
def put_redirect_permanent():
    return redirect('/get/permanent_redirected', code=301)

# 302
@app.route("/put/redirect", methods = ['PUT'])
def put_redirect():
    return redirect('/get/redirected', code=302)

# 303
@app.route("/put/redirect_303", methods = ['PUT'])
def put_redirect_303():
    return redirect('/get/redirected', code=303)

# 307
@app.route("/put/redirect_307", methods = ['PUT'])
def put_redirect_307():
    return redirect('/put/redirected', code=307)

# 308
@app.route("/put/redirect_308", methods = ['PUT'])
def put_redirect_308():
    return redirect('/put/permanent_redirected', code=308)


@app.route("/put/bla", methods = ['PUT'])
def put_bla():

    if request.content_type != "text/html":
        return Response("", status=400)

    print("data", request.data)
    return request.data

@app.route("/post/bla", methods = ['POST'])
def post_bla():
    return "Got data of type " + request.content_type + ": " + request.data.decode('utf-8')

# @app.route("/post/redirect", methods = ['POST'])
# def post_redirect():
#     return redirect("/post/bla", code=308)

@app.route("/cookies/none", methods = ['GET'])
def cookies_none():
    data = "Hello\n"

    resp = Response(data, mimetype='text/plain')

    return resp

@app.route("/cookies/somecookie", methods = ['GET'])
def cookies_somecookie():
    data = "Hello\n"

    resp = Response(data, mimetype='text/plain')

    expires_date = datetime.datetime(2045, 5, 13, 18, 52, 0)

    resp.set_cookie('userID', value="1234",  expires=expires_date, path="/get/calendar", domain="localhost", secure=False, httponly=True)

    return resp

@app.route("/cookies/twocookies", methods = ['GET'])
def cookies_twocookies():
    data = "Hello\n"

    resp = Response(data, mimetype='text/plain')

    expires_date = datetime.datetime(2045, 5, 13, 18, 52, 0)

    resp.set_cookie('userID', value="1234", expires=expires_date, path="/get/calendar", domain="localhost", secure=False, httponly=True)
    resp.set_cookie('konqi', value="Yo",  expires=expires_date, path="/get/text", domain="localhost", secure=False, httponly=True)

    return resp

@app.route("/cookies/showsent", methods = ['GET'])
def cookies_showsent():
    data = ""
    for key in request.cookies:
        data += key + ":" + request.cookies[key] + "\n"

    resp = Response(data, mimetype='text/plain')
    return resp

@auth.verify_password
def verify_password(username, password):
    return username == "nico" and password == "1234"

@app.route('/auth/test')
@auth.login_required
def index():
    return "Hello"

@app.route("/useragent/enforce")
def useragent_enforce():
    ua = request.headers.get('User-Agent')

    print("ua:", ua)

    if ua == "my test UA":
        return "Hello"
    else:
        return Response("", status=400)

@app.route("/error/no", methods = ['GET', 'PUT'])
def error_no():
    return "Hello"

@app.route("/error/400", methods = ['GET', 'PUT'])
def error_400():
    return Response("", status=400)

@app.route("/error/403", methods = ['GET', 'PUT'])
def error_403():
    return Response("", status=403)

@app.route("/error/405", methods = ['GET', 'PUT'])
def error_405():
    return Response("", status=405)

@app.route("/error/451", methods = ['GET', 'PUT'])
def error_451():
    return Response("", status=451)

@app.route("/error/500", methods = ['GET', 'PUT'])
def error_500():
    return Response("", status=500)

@app.route("/error/502", methods = ['GET', 'PUT'])
def error_502():
    return Response("", status=502)

@app.route("/error/507", methods = ['GET', 'PUT'])
def error_507():
    return Response("", status=507)

@app.route("/accept/rss")
def accept_rss():

    accept = request.headers.get('Accept')

    if accept == "application/rss+xml;q=0.9, application/atom+xml;q=0.9, text/*;q=0.8, */*;q=0.7":
        return "Hello"
    else:
        return Response("", status=400)

@app.route("/referrer/test")
def referrer_test():

    if request.referrer == "http://kde.org":
        return "Hello"
    else:
        return Response("", status=400)

@app.route("/headers/pineapple")
def headers_ananas():
    pineapple = request.headers.get('Pineapple')

    if pineapple == "Ananas":
        return "Hello"
    else:
        return Response("", status=400)

@app.route("/headers/pizza")
def headers_pizza():
    pineapple = request.headers.get('Pineapple')
    pizza = request.headers.get('Pizza')

    if pineapple == "Ananas" and pizza == "yes":
        return "🤌"
    else:
        return Response("", status=400)

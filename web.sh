#!/bin/sh
socat TCP4-LISTEN:8080,fork EXEC:./web

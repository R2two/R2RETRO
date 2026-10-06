.PHONY: all ps4 desktop test
all: ps4
ps4:
	bash scripts/build.sh ps4
desktop:
	bash scripts/build.sh desktop
test:
	bash scripts/build.sh test


all:
	make -C build -f makefile_litets
	make -C build -f makefile_demo

clean:
	make -C build -f makefile_litets clean
	make -C build -f makefile_demo clean

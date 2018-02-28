all:
	gcc -g -O3 -pthread -o main main.c lwt_asm.S
clean:
	rm main

TARGET=ft_ping

SRC=ping.c argp.c utils.c cli.c

OBJ_DIR=objs
OBJ=$(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))
arg ?= -V


all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(OBJ) -o $(TARGET) -lm

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	gcc -Wall -Werror -Wextra -c $< -o $@

.PHONY: all clean fclean run test

run: $(TARGET)
	@./$(TARGET) $(arg)

test_ref:
	@docker build --platform linux/amd64 -t inetutils:2.0 .
	@docker run -it inetutils:2.0 bash -c "ping $(arg)"

test:
	@docker build --platform linux/amd64 -f Dockerfile.test -t ft_ping:test .
	@docker run --platform linux/amd64 --rm --init -it ft_ping:test /app/ft_ping $(arg)

valgrind:
	@docker build --platform linux/amd64 -f Dockerfile.test -t ft_ping:test .
	@docker run --platform linux/amd64 --rm --init -it --ulimit nofile=1048576:1048576 --cap-add=NET_RAW --cap-add=SYS_PTRACE --security-opt seccomp=unconfined ft_ping:test valgrind --leak-check=full ./ft_ping $(arg)

clean:
	rm -f $(OBJ) $(TARGET)

fclean: clean
	rm -f $(TARGET)

TARGET=ft_ping

SRC=ping.c

OBJ_DIR=objs
OBJ=$(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))
arg ?= -V


all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(OBJ) -o $(TARGET)

$(OBJ): $(SRC)
	@mkdir -p $(OBJ_DIR)
	gcc -Wall -Werror -Wextra -c $(SRC) -o $(OBJ_DIR)/$(@F)

.PHONY: all clean fclean run

run: $(TARGET)
	@./$(TARGET)

test_ref:
	@docker build --platform linux/amd64 -t inetutils:2.0 .
	@docker run -it inetutils:2.0 bash -c "ping $(arg)"

clean:
	rm -f $(OBJ) $(TARGET)

fclean: clean
	rm -f $(TARGET)
say_hello:
	        @echo "Hello World"

generate:	
		@echo "generating...."
		@gcc project_2.c -lpthread
clean:
	        @echo "Cleaning up..."
		@rm ./a.out
run:
		@echo "running..."
		@./a.out

# Makefile simples para Disaster Analysis Dashboard

CC = gcc
CFLAGS = -Wall -std=c99
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

disaster_analysis: main.c disaster_star_schema.c star_schema_indexes.c trie.c bplus.c
	$(CC) $(CFLAGS) -o disaster_analysis main.c disaster_star_schema.c star_schema_indexes.c trie.c bplus.c $(LIBS)

clean:
	rm -f disaster_analysis

run: disaster_analysis
	./disaster_analysis

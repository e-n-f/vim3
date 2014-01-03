doctags: doctags.c
	cc doctags.c -o doctags

DOCS = reference.doc windows.doc

tags: $(DOCS)
	./doctags $(DOCS) | sort >tags

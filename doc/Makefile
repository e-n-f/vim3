DOCS = vim_ami.txt vim_arch.txt vim_diff.txt vim_digr.txt vim_dos.txt \
	vim_help.txt vim_idx.txt vim_mac.txt vim_nt.txt vim_ref.txt \
	vim_tips.txt vim_unix.txt vim_win.txt

vim_tags: doctags $(DOCS)
	./doctags $(DOCS) | sort >vim_tags

doctags: doctags.c
	cc doctags.c -o doctags

clean:
	-rm doctags

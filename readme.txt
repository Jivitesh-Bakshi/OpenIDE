Neotex is at the initial stage of development. it is a simple program using gap buffer and dynamic memory allocation to operate 
as a functioning terminal based text editor. It is basic, and has very little overhead. 
It can create files with different extensions, open them f0r editting and has autoindentation. It is NOT an IDE itself, and has no syntax highlighting yet. 

This is a small hobby project with the main aim being, understanding memory allocation and file manipulation with C. 

Neotex operates using commands f0r cursor positioning, line deletion, saving files and exitting. 

:m L --> moves cursor to the start or line L
:d L --> deletes line L
:d *L --> deletes line L and everything afterwards. 
:d *L M* deletes everything between line L and M (inclusive).
:w --> saves the program without without editting.
ESVA --> saves the projects and exits neotex.

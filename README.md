# authorOrganizer

The purpose of this little program is to organize book files into a new folder (although you can use it for
any type of sorting based on filenames), keeping the original folder in the exact same state as it was, 
but without having to duplicate files.

I use it because I use a tool to auto-download books into a specific folder and part of the way that is done is
by keeping an index of the files in that folder so that it knows if it needs to download the
book again or not (I don't control the tool that does it though, or have access to its code).
At the same time, since it can download some thousands of books at once and I wanted to be able to quickly 
organize them by author, I decided to build this tool. This way it quickly searches/organizes books so I can
share them with others easily.

The way this works is I input an author name then modify it to account for file naming differences,
like spaces (or lack of), capital letters, accent marks and separators. I then search all the files and 
filter them. If I have matches, I create a new folder named with the initial input, move all the files to it 
and do some title formatting. For each book I move, I leave a hard link to it in its place with the original name.
In the end I send a summary of the process to the screen and any errors that might have happened.


Things to improve:

- It always rechecks every single book, I can add an option to skip certain books, although I might want to organize
them by different criteria.

- Add option whether to create a hard link or not.

- I can create a list of authors and then automatically do the sorting for each name on the list.

- Name formatting on the moved data.

- Keep some data informing whether or not I sorted the data before and asking if I want to resort it again,
  this to allow me to skip certain books in the next checks.

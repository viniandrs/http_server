# check if argument "webspace_dir" is given
if [ -z "$1" ]; then
  echo "Usage: $0 <webspace_dir>"
  exit 1
fi

# giving complete permissions to all files and directories in webspace_dir
chmod 777 -R $1

# removing read permission for file teste.txt in webspace_dir
chmod ugo-r $1/teste.txt

# removing execution permission for directory dir in webspace_dir
chmod ugo-x $1/dir

# removing read permission for directory dir3 in webspace_dir
chmod ugo-r $1/dir3/index.html 

# removing read permission for index.html and welcome.html in webspace_dir/dir4
chmod ugo-r $1/dir4/index.html
chmod ugo-r $1/dir4/welcome.html


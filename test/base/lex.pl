$a = 0; { $a = 14; } $a #=> 0 #TODO =>14
# inner block should access outer var, only with <my $a> make it private

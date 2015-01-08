;;; (autoload 'potion-mode "potion-mode" nil t)
;;; (add-to-list 'auto-mode-alist '("\\.pn$" . potion-mode))

(eval-when-compile
  (require 'generic)
  (require 'font-lock)
  (require 'regexp-opt))

(defmacro potion-match-symbol (&rest symbols)
  "Convert a word-list into a font-lock regexp."
  (concat "\\_<" (regexp-opt symbols t) "\\_>"))

(define-generic-mode potion-mode
  '(?#)                                    ;comments
  '("and" "or" "not" "nil" "true" "false") ;keywords
  `(                                       ;font-lock-list
    ;; block delimiters
    ("[.:]" . font-lock-preprocessor-face)
    ;; "licks" (data language)
    ("[][]" . font-lock-warning-face)
    (,(concat
       ;; one-char operators
       "[,()|?=+~*%<>=!&^-]"
       ;; multi-char operators
       "\\|\\("
       "\\+\\+\\|--\\|\\*\\*\\|<<\\|>>\\|<=\\|>=\\|==\\|!=\\|<=>\\|&&\\|||"
       "\\)")
     . font-lock-builtin-face)
    ;; slash is magical
    ("\\(/\\) " 1 font-lock-builtin-face)
    ;; numeric constants
    ("\\_<[0-9]+\\(\\.[0-9]*\\)?\\([Ee][+-]?[0-9]+\\)?\\_>" . font-lock-constant-face)
    ("0x[a-fA-F0-9]+" . font-lock-constant-face)
    ;; attributes
    ("/\\(?:\\sw\\|\\s_\\)+\\_>" . font-lock-variable-name-face)
    ;; control constructs
    (,(potion-match-symbol
       "class" "if" "elsif" "else" "loop" "while" "to" "times" "return")
     . font-lock-keyword-face)
    ;; core functions (XXX some overlap with operators)
    (,(potion-match-symbol 
       "%" "*" "**" "+" "++" "-" "--" "/" "<<" ">>" "about" "abs" "append" "arity"
       "at" "attr" "bytes" "call" "chr" "clone" "close" "code" "compile"
       "double" "double?" "each" "eval" "exit" "first" "forward" "here"
       "integer" "integer?" "join" "kind" "last" "length" "licks" "list" "load" "meta"
       "name" "nil" "nil?" "number" "number?" "ord" "pop" "print" "push" "put" "rand"
       "read" "remove" "reverse" "self" "send" "slice" "sqrt" "srand" "step" "string"
       "string?" "text" "times" "to" "tree" "write" "~") . font-lock-builtin-face)
    )
  '("\\.pn$")                           ;file extension
  '((lambda ()                          ;other setup work
      (modify-syntax-entry ?' "\"")
      (modify-syntax-entry ?: "(.")
      (modify-syntax-entry ?\. "):")))
  "Major mode for editing _why's Potion language."
)

(provide 'portion-mode)

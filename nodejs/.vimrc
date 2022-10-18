syn cluster codeComment contains=javaScriptLineComment,javaScriptComment,cComment,cCommentL

" assert / log
hi Debug ctermfg=13 ctermbg=0
syn match Debug /^assert\>/
syn keyword Debug contained assert containedin=@codeComment
hi link Log Debug
syn match Log /^_\{0,1}\(quiet\|warn\|info\|verbose\|trace\|debug\|BUG\|CONFLICT\)\>/
syn keyword Log contained quiet warn info verbose trace debug BUG containedin=@codeComment

" annotation (*)syntax/javascript.vim expansion
hi Annotation term=standout ctermfg=13 ctermbg=0 guifg=Blue guibg=Black
hi link javaScriptCommentTodo Annotation
syn keyword javaScriptCommentTodo contained BUG BUGFIX FIXED NOTE OBSOLETE PENDING WARNING containedin=javaScriptLineComment,javaScriptComment

" development
hi Development ctermfg=DarkYellow ctermbg=0
syn keyword Development archive buffers namehash strings locrectab locrec locrec_offset sumhash wcwidth head table3_lookup table3_data table3 old_table3

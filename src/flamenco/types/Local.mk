ifdef FD_HAS_INT128
$(call add-hdrs,fd_bincode.h fd_types.h fd_types_custom.h fd_types_meta.h)
$(call add-objs,fd_types,fd_flamenco)
$(call make-unit-test,test_types_meta,test_types_meta,fd_flamenco fd_ballet fd_util)
endif

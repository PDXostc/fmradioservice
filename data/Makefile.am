EXTRA_DIST = fmradioservice.service.in com.jlr.fmradioservice.service
CLEANFILES =

if HAVE_SYSTEMD
unit_DATA = fmradioservice.service
CLEANFILES += $(unit_DATA)
endif

dbusdir = $(datadir)/dbus-1/services
dbus_DATA = com.jlr.fmradioservice.service
CLEANFILES += $(service_DATA)

fmradioservice.service: fmradioservice.service.in
	$(AM_V_GEN) $(SED) -e 's|@bindir[@]|$(bindir)|g' $< > $@.tmp && mv $@.tmp $@

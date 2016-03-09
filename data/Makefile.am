EXTRA_DIST = fmradioservice.service.in com.jlr.fmradioservice.service
CLEANFILES =

if HAVE_SYSTEMD
systemduserunit_DATA = fmradioservice.service
CLEANFILES += $(systemduserunit_DATA)
endif

dbusdir = $(datadir)/dbus-1/services
dbus_DATA = com.jlr.fmradioservice.service
CLEANFILES += $(service_DATA)

fmradioservice.service: fmradioservice.service.in
	$(AM_V_GEN) $(SED) -e 's|@bindir[@]|$(bindir)|g' $< > $@.tmp && mv $@.tmp $@

com.jlr.fmradioservice.service: com.jlr.fmradioservice.service.in
	$(AM_V_GEN) $(SED) -e 's|@bindir[@]|$(bindir)|g' $< > $@.tmp && mv $@.tmp $@

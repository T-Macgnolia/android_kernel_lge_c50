/*
 *  Sysfs interface for the universal power supply monitor class
 *
 *  Copyright © 2007  David Woodhouse <dwmw2@infradead.org>
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/stat.h>

#include "power_supply.h"
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
#ifdef CONFIG_64BIT
#include <soc/qcom/lge/board_lge.h>
#else
#include <mach/board_lge.h>
#endif
#endif

/*
 * This is because the name "current" breaks the device attr macro.
 * The "current" word resolves to "(get_current())" so instead of
 * "current" "(get_current())" appears in the sysfs.
 *
 * The source of this definition is the device.h which calls __ATTR
 * macro in sysfs.h which calls the __stringify macro.
 *
 * Only modification that the name is not tried to be resolved
 * (as a macro let's say).
 */

#define POWER_SUPPLY_ATTR(_name)					\
{									\
	.attr = { .name = #_name },					\
	.show = power_supply_show_property,				\
	.store = power_supply_store_property,				\
}

#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
#define PSEUDO_BATT_ATTR(_name)						\
{									\
	.attr = { .name = #_name, .mode = 0644},			\
	.show = pseudo_batt_show_property,				\
	.store = pseudo_batt_store_property,				\
}
#endif

#ifdef CONFIG_LGE_PM_LLK_MODE
#define STORE_DEMO_ENABLED_ATTR(_name)                  \
{                                   \
	    .attr = { .name = #_name, .mode = 0644},            \
	    .show =  power_supply_show_property,                \
	    .store =  power_supply_store_property,              \
}
#endif

static struct device_attribute power_supply_attrs[];

static ssize_t power_supply_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf) {
	static char *type_text[] = {
		"Unknown", "Battery", "UPS", "Mains", "USB",
		"USB_DCP", "USB_CDP", "USB_ACA", "Wireless", "BMS",
#ifdef CONFIG_LGE_PM_BATTERY_EXTERNAL_FUELGAUGE
		"External_Fuelgauge",
#endif
#ifdef CONFIG_CHG_DETECTOR_MAX14656
		"Max14656",
#endif
		"USB_Parallel"
	};
	static char *status_text[] = {
		"Unknown", "Charging", "Discharging", "Not charging", "Full"
	};
	static char *charge_type[] = {
		"Unknown", "N/A", "Trickle", "Fast", "Taper"
	};
	static char *health_text[] = {
		"Unknown", "Good", "Overheat", "Warm", "Dead", "Over voltage",
		"Unspecified failure", "Cold", "Cool", "Watchdog timer expire",
		"Safety timer expire"
	};
	static char *technology_text[] = {
		"Unknown", "NiMH", "Li-ion", "Li-poly", "LiFe", "NiCd",
		"LiMn"
	};
	static char *capacity_level_text[] = {
		"Unknown", "Critical", "Low", "Normal", "High", "Full"
	};
	static char *scope_text[] = {
		"Unknown", "System", "Device"
	};

	ssize_t ret = 0;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;

	if (off == POWER_SUPPLY_PROP_TYPE)
		value.intval = psy->type;
	else
		ret = psy->get_property(psy, off, &value);

	if (ret < 0) {
		if (ret == -ENODATA)
			dev_dbg(dev, "driver has no data for `%s' property\n",
				attr->attr.name);
		else if (ret != -ENODEV)
			dev_err(dev, "driver failed to report `%s' property: %zd\n",
				attr->attr.name, ret);
		return ret;
	}

	if (off == POWER_SUPPLY_PROP_STATUS)
		return sprintf(buf, "%s\n", status_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CHARGE_TYPE)
		return sprintf(buf, "%s\n", charge_type[value.intval]);
	else if (off == POWER_SUPPLY_PROP_HEALTH)
		return sprintf(buf, "%s\n", health_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TECHNOLOGY)
		return sprintf(buf, "%s\n", technology_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CAPACITY_LEVEL)
		return sprintf(buf, "%s\n", capacity_level_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TYPE)
		return sprintf(buf, "%s\n", type_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_SCOPE)
		return sprintf(buf, "%s\n", scope_text[value.intval]);
	else if (off >= POWER_SUPPLY_PROP_MODEL_NAME)
		return sprintf(buf, "%s\n", value.strval);
#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
	else if (off == POWER_SUPPLY_PROP_HW_REV)
		return sprintf(buf, "%s\n", value.strval);
#endif

	if (off == POWER_SUPPLY_PROP_CHARGE_COUNTER_EXT)
		return sprintf(buf, "%lld\n", value.int64val);
	else
		return sprintf(buf, "%d\n", value.intval);
}

static ssize_t power_supply_store_property(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count) {
	ssize_t ret;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;
	long long_val;

	/* TODO: support other types than int */
	ret = strict_strtol(buf, 10, &long_val);
	if (ret < 0)
		return ret;

	value.intval = long_val;

	ret = psy->set_property(psy, off, &value);
	if (ret < 0)
		return ret;

	return count;
}

#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
static ssize_t pseudo_batt_show_property(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t ret;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;

	static char *pseudo_batt[] = {
		"NORMAL", "PSEUDO",
	};

	ret = psy->get_property(psy, off, &value);

	if (ret < 0) {
		if (ret != -ENODEV)
			dev_err(dev, "driver failed to report `%s' property\n",
					attr->attr.name);
		return ret;
	}
	if (off == POWER_SUPPLY_PROP_PSEUDO_BATT)
		return sprintf(buf, "[%s] \nusage: echo [mode] [ID] [therm] [temp] [volt] [cap] [charging] > pseudo_batt\n",
				pseudo_batt[value.intval]);

	return 0;
}

static ssize_t pseudo_batt_store_property(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = -EINVAL;
	struct pseudo_batt_info_type info;

	if (sscanf(buf, "%d %d %d %d %d %d %d", &info.mode, &info.id, &info.therm,
				&info.temp, &info.volt, &info.capacity, &info.charging) != 7)
	{
		if(info.mode == 1) //pseudo mode
		{
			printk(KERN_ERR "usage : echo [mode] [ID] [therm] [temp] [volt] [cap] [charging] > pseudo_batt");
			goto out;
		}
	}
	pseudo_batt_set(&info);
	ret = count;

out:
	return ret;
}
#endif

/* Must be in the same order as POWER_SUPPLY_PROP_* */
static struct device_attribute power_supply_attrs[] = {
	/* Properties of type `int' */
	POWER_SUPPLY_ATTR(status),
	POWER_SUPPLY_ATTR(charge_type),
	POWER_SUPPLY_ATTR(health),
	POWER_SUPPLY_ATTR(present),
	POWER_SUPPLY_ATTR(online),
	POWER_SUPPLY_ATTR(authentic),
	POWER_SUPPLY_ATTR(charging_enabled),
	POWER_SUPPLY_ATTR(technology),
	POWER_SUPPLY_ATTR(cycle_count),
	POWER_SUPPLY_ATTR(voltage_max),
	POWER_SUPPLY_ATTR(voltage_min),
	POWER_SUPPLY_ATTR(voltage_max_design),
	POWER_SUPPLY_ATTR(voltage_min_design),
	POWER_SUPPLY_ATTR(voltage_now),
	POWER_SUPPLY_ATTR(voltage_avg),
	POWER_SUPPLY_ATTR(voltage_ocv),
	POWER_SUPPLY_ATTR(input_voltage_regulation),
	POWER_SUPPLY_ATTR(current_max),
	POWER_SUPPLY_ATTR(input_current_max),
	POWER_SUPPLY_ATTR(input_current_trim),
	POWER_SUPPLY_ATTR(input_current_settled),
	POWER_SUPPLY_ATTR(bypass_vchg_loop_debouncer),
	POWER_SUPPLY_ATTR(current_now),
	POWER_SUPPLY_ATTR(current_avg),
	POWER_SUPPLY_ATTR(power_now),
	POWER_SUPPLY_ATTR(power_avg),
	POWER_SUPPLY_ATTR(charge_full_design),
	POWER_SUPPLY_ATTR(charge_empty_design),
	POWER_SUPPLY_ATTR(charge_full),
	POWER_SUPPLY_ATTR(charge_empty),
	POWER_SUPPLY_ATTR(charge_now),
	POWER_SUPPLY_ATTR(charge_avg),
	POWER_SUPPLY_ATTR(charge_counter),
	POWER_SUPPLY_ATTR(charge_counter_shadow),
	POWER_SUPPLY_ATTR(constant_charge_current),
	POWER_SUPPLY_ATTR(constant_charge_current_max),
	POWER_SUPPLY_ATTR(constant_charge_voltage),
	POWER_SUPPLY_ATTR(constant_charge_voltage_max),
	POWER_SUPPLY_ATTR(charge_control_limit),
	POWER_SUPPLY_ATTR(charge_control_limit_max),
	POWER_SUPPLY_ATTR(energy_full_design),
	POWER_SUPPLY_ATTR(energy_empty_design),
	POWER_SUPPLY_ATTR(energy_full),
	POWER_SUPPLY_ATTR(energy_empty),
	POWER_SUPPLY_ATTR(energy_now),
	POWER_SUPPLY_ATTR(energy_avg),
	POWER_SUPPLY_ATTR(hi_power),
	POWER_SUPPLY_ATTR(low_power),
	POWER_SUPPLY_ATTR(capacity),
	POWER_SUPPLY_ATTR(capacity_alert_min),
	POWER_SUPPLY_ATTR(capacity_alert_max),
	POWER_SUPPLY_ATTR(capacity_level),
	POWER_SUPPLY_ATTR(temp),
	POWER_SUPPLY_ATTR(temp_alert_min),
	POWER_SUPPLY_ATTR(temp_alert_max),
	POWER_SUPPLY_ATTR(temp_cool),
	POWER_SUPPLY_ATTR(temp_warm),
	POWER_SUPPLY_ATTR(temp_ambient),
	POWER_SUPPLY_ATTR(temp_ambient_alert_min),
	POWER_SUPPLY_ATTR(temp_ambient_alert_max),
	POWER_SUPPLY_ATTR(time_to_empty_now),
	POWER_SUPPLY_ATTR(time_to_empty_avg),
	POWER_SUPPLY_ATTR(time_to_full_now),
	POWER_SUPPLY_ATTR(time_to_full_avg),
	POWER_SUPPLY_ATTR(type),
	POWER_SUPPLY_ATTR(scope),
	POWER_SUPPLY_ATTR(system_temp_level),
	POWER_SUPPLY_ATTR(resistance),
	POWER_SUPPLY_ATTR(resistance_capacitive),
	POWER_SUPPLY_ATTR(resistance_id),
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	POWER_SUPPLY_ATTR(battery_id_checker),
#endif
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
	PSEUDO_BATT_ATTR(pseudo_batt),
#endif
#ifdef CONFIG_LGE_PM_USB_CURRENT_MAX
	POWER_SUPPLY_ATTR(usb_current_max),
#endif
#ifdef CONFIG_LGE_PM
	POWER_SUPPLY_ATTR(safety_timer),
#endif
#ifdef CONFIG_LGE_PM_CHARGING_BQ24296_CHARGER
	POWER_SUPPLY_ATTR(ext_pwr),
	POWER_SUPPLY_ATTR(removed),
#elif defined (CONFIG_LGE_PM_CHARGING_BQ24262_CHARGER)
	POWER_SUPPLY_ATTR(ext_pwr),
#endif
#ifdef CONFIG_LGE_PM_CHARGING_VZW_POWER_REQ
	POWER_SUPPLY_ATTR(vzw_chg),
#endif
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	POWER_SUPPLY_ATTR(valid_batt_id),
#endif
#if defined (CONFIG_LGE_PM_CHARGING_BQ24296_CHARGER) || defined (CONFIG_LGE_PM_CHARGING_BQ24262_CHARGER)
        POWER_SUPPLY_ATTR(charger_timer),
	POWER_SUPPLY_ATTR(charging_complete),
#endif
#if defined (CONFIG_LGE_PM_BATTERY_EXTERNAL_FUELGAUGE)
        POWER_SUPPLY_ATTR(use_fuelgauge),
#endif
#ifdef CONFIG_CHG_DETECTOR_MAX14656
	POWER_SUPPLY_ATTR(usb_chg_detect_done),
	POWER_SUPPLY_ATTR(usb_chg_type),
        POWER_SUPPLY_ATTR(usb_dcd_timeout),
	POWER_SUPPLY_ATTR(usb_chg_type_manual),
#endif
#if defined(CONFIG_LGE_PM_LLK_MODE)
	STORE_DEMO_ENABLED_ATTR(store_demo_enabled),
#endif
#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
	POWER_SUPPLY_ATTR(hw_rev),
#endif
#ifdef CONFIG_LGE_PM
	POWER_SUPPLY_ATTR(calculated_soc),
#endif
#ifdef CONFIG_LGE_PM_BATTERY_RT9428_EOC_BY_SOC
	POWER_SUPPLY_ATTR(status_raw),
	POWER_SUPPLY_ATTR(capacity_raw),
#endif
	/* Local extensions */
	POWER_SUPPLY_ATTR(usb_hc),
	POWER_SUPPLY_ATTR(usb_otg),
	POWER_SUPPLY_ATTR(charge_enabled),
	POWER_SUPPLY_ATTR(flash_current_max),
	/* Local extensions of type int64_t */
	POWER_SUPPLY_ATTR(charge_counter_ext),
	/* Properties of type `const char *' */
	POWER_SUPPLY_ATTR(model_name),
	POWER_SUPPLY_ATTR(manufacturer),
	POWER_SUPPLY_ATTR(serial_number),
	POWER_SUPPLY_ATTR(battery_type),
};

static struct attribute *
__power_supply_attrs[ARRAY_SIZE(power_supply_attrs) + 1];

static umode_t power_supply_attr_is_visible(struct kobject *kobj,
					   struct attribute *attr,
					   int attrno)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct power_supply *psy = dev_get_drvdata(dev);
	umode_t mode = S_IRUSR | S_IRGRP | S_IROTH;
	int i;

	if (attrno == POWER_SUPPLY_PROP_TYPE)
		return mode;

	for (i = 0; i < psy->num_properties; i++) {
		int property = psy->properties[i];

		if (property == attrno) {
			if (psy->property_is_writeable &&
			    psy->property_is_writeable(psy, property) > 0)
				mode |= S_IWUSR;

			return mode;
		}
	}

	return 0;
}

static struct attribute_group power_supply_attr_group = {
	.attrs = __power_supply_attrs,
	.is_visible = power_supply_attr_is_visible,
};

static const struct attribute_group *power_supply_attr_groups[] = {
	&power_supply_attr_group,
	NULL,
};

void power_supply_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = power_supply_attr_groups;

	for (i = 0; i < ARRAY_SIZE(power_supply_attrs); i++)
		__power_supply_attrs[i] = &power_supply_attrs[i].attr;
}

static char *kstruprdup(const char *str, gfp_t gfp)
{
	char *ret, *ustr;

	ustr = ret = kmalloc(strlen(str) + 1, gfp);

	if (!ret)
		return NULL;

	while (*str)
		*ustr++ = toupper(*str++);

	*ustr = 0;

	return ret;
}

int power_supply_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	int ret = 0, j;
	char *prop_buf;
	char *attrname;

	dev_dbg(dev, "uevent\n");

	if (!psy || !psy->dev) {
		dev_dbg(dev, "No power supply yet\n");
		return ret;
	}

	dev_dbg(dev, "POWER_SUPPLY_NAME=%s\n", psy->name);

	ret = add_uevent_var(env, "POWER_SUPPLY_NAME=%s", psy->name);
	if (ret)
		return ret;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return -ENOMEM;

	for (j = 0; j < psy->num_properties; j++) {
		struct device_attribute *attr;
		char *line;

		attr = &power_supply_attrs[psy->properties[j]];

		ret = power_supply_show_property(dev, attr, prop_buf);
		if (ret == -ENODEV || ret == -ENODATA) {
			/* When a battery is absent, we expect -ENODEV. Don't abort;
			   send the uevent with at least the the PRESENT=0 property */
			ret = 0;
			continue;
		}

		if (ret < 0)
			goto out;

		line = strchr(prop_buf, '\n');
		if (line)
			*line = 0;

		attrname = kstruprdup(attr->attr.name, GFP_KERNEL);
		if (!attrname) {
			ret = -ENOMEM;
			goto out;
		}

		dev_dbg(dev, "prop %s=%s\n", attrname, prop_buf);

		ret = add_uevent_var(env, "POWER_SUPPLY_%s=%s", attrname, prop_buf);
		kfree(attrname);
		if (ret)
			goto out;
	}

out:
	free_page((unsigned long)prop_buf);

	return ret;
}
